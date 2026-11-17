import { test } from "node:test";
import assert from "node:assert/strict";
import { chartSegments, isStale, visibleValue } from "../src/presentation.ts";
import { recordSchema } from "../src/api.ts";
import type { Channel, Reading } from "../src/api.ts";

const channel: Channel = {
  quality: "VALID",
  valid: true,
  unit: "relative_index",
  source: "simulator",
  value: 20,
};

function reading(seconds: number): Reading {
  return {
    id: seconds,
    device_id: "vigia-sim-01",
    boot_id: "test",
    seq: seconds,
    received_at: new Date(Date.UTC(2026, 10, 16, 12, 0, seconds)).toISOString(),
    payload: {
      message_kind: "telemetry",
      simulated: true,
      assessment_state: "NO_CHANGE",
      assessment_available: true,
      filtered_valid: true,
      optical_index_filtered: 20,
      test_override: true,
      channels: {
        optical: channel,
        temperature: channel,
        flow: channel,
        pressure: channel,
      },
    },
  };
}

test("leituras antigas ou inválidas não aparecem como valores atuais", () => {
  assert.equal(visibleValue(channel, false), "20");
  assert.equal(visibleValue(channel, true), "—");
  assert.equal(
    visibleValue({ ...channel, quality: "INVALID", valid: false }, false),
    "—",
  );
  assert.equal(visibleValue(undefined, false), "—");
});

test("a comunicacao usa a idade do servidor e continua envelhecendo sem respostas", () => {
  assert.equal(isStale(2_000, 20, 10_000), false);
  assert.equal(isStale(2_000, 20, 19_000), true);
  assert.equal(isStale(21_000, 20, 0), true);
  assert.equal(isStale(null, 20, 0), true);
  assert.equal(isStale(undefined, 20, 0), true);
});

test("a validade independe do relogio de parede do computador", (t) => {
  for (const skew of [-10_800_000, 10_800_000]) {
    t.mock.method(Date, "now", () => Date.UTC(2026, 10, 17) + skew);
    assert.equal(isStale(1_000, 20, 5_000), false);
    assert.equal(isStale(1_000, 20, 20_000), true);
    t.mock.restoreAll();
  }
});

test("o gráfico não inventa valores nem conecta trechos inválidos", () => {
  assert.deepEqual(chartSegments([]), []);
  const invalid = reading(1);
  invalid.payload.filtered_valid = false;
  invalid.payload.optical_index_filtered = null;
  assert.equal(chartSegments([reading(0), invalid, reading(2)]).length, 2);
  assert.equal(chartSegments([reading(2), reading(0)]).length, 1);
});

test("o contrato do dashboard rejeita uma classificação desconhecida", () => {
  const input: unknown = {
    ...reading(0),
    payload: { ...reading(0).payload, assessment_state: "POTABLE" },
  };
  assert.equal(recordSchema.safeParse(input).success, false);
});
