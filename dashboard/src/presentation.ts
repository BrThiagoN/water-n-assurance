import type { Channel, Reading, WaterState } from "./api.ts";

export const stateCopy: Record<
  WaterState,
  { title: string; description: string; tone: string }
> = {
  NO_CHANGE: {
    title: "Sem alteração detectada",
    description:
      "Os parâmetros acompanhados não indicam uma mudança confirmada neste momento.",
    tone: "calm",
  },
  ATTENTION: {
    title: "Atenção à mudança",
    description:
      "O aparelho percebeu uma alteração. Acompanhe os próximos registros e verifique o ponto monitorado.",
    tone: "attention",
  },
  CHECK: {
    title: "Verifique a água",
    description:
      "Uma mudança persistente foi sinalizada. Registre o ocorrido e solicite uma avaliação adequada.",
    tone: "check",
  },
  UNAVAILABLE: {
    title: "Leitura indisponível",
    description:
      "Não há informação suficiente para avaliar a alteração agora. Verifique o aparelho e aguarde novas leituras.",
    tone: "unavailable",
  },
};

export const dateTime = (date: string): string =>
  new Intl.DateTimeFormat("pt-BR", {
    day: "2-digit",
    month: "short",
    hour: "2-digit",
    minute: "2-digit",
    timeZone: "America/Sao_Paulo",
  }).format(new Date(date));
export const clockTime = (date: string): string =>
  new Intl.DateTimeFormat("pt-BR", {
    hour: "2-digit",
    minute: "2-digit",
    timeZone: "America/Sao_Paulo",
  }).format(new Date(date));
export const number = (value: number): string =>
  new Intl.NumberFormat("pt-BR", { maximumFractionDigits: 1 }).format(value);

// A idade vem do servidor; o tempo adicional usa performance.now().
// O relogio de parede do computador pode estar adiantado ou atrasado.
export function isStale(
  serverAgeMs: number | null | undefined,
  thresholdSeconds: number,
  elapsedMs: number,
): boolean {
  return (
    serverAgeMs == null ||
    serverAgeMs + Math.max(0, elapsedMs) > thresholdSeconds * 1000
  );
}

export function visibleValue(
  channel: Channel | undefined,
  stale: boolean,
): string {
  return !stale && channel?.valid && typeof channel.value === "number"
    ? number(channel.value)
    : "—";
}

export function chartSegments(
  readings: Reading[],
): { points: string; start: string; end: string }[] {
  const ordered = [...readings].sort(
    (a, b) => Date.parse(a.received_at) - Date.parse(b.received_at),
  );
  const result: { points: string; start: string; end: string }[] = [];
  let points: string[] = [];
  let start = "";
  let end = "";
  const flush = (): void => {
    if (points.length) result.push({ points: points.join(" "), start, end });
    points = [];
  };
  const first = ordered[0];
  const last = ordered.at(-1);
  if (!first || !last) return result;
  const from = Date.parse(first.received_at);
  const span = Math.max(1, Date.parse(last.received_at) - from);
  for (const row of ordered) {
    const value = row.payload.optical_index_filtered;
    if (
      !row.payload.filtered_valid ||
      typeof value !== "number" ||
      !row.payload.channels.optical.valid
    ) {
      flush();
      continue;
    }
    if (!points.length) start = row.received_at;
    end = row.received_at;
    const x =
      ordered.length === 1
        ? 480
        : 50 + ((Date.parse(row.received_at) - from) / span) * 850;
    points.push(`${x},${190 - Math.min(100, Math.max(0, value)) * 1.6}`);
  }
  flush();
  return result;
}
