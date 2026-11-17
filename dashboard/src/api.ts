import { z } from "zod";

export const stateSchema = z.enum([
  "NO_CHANGE",
  "ATTENTION",
  "CHECK",
  "UNAVAILABLE",
]);
export type WaterState = z.infer<typeof stateSchema>;
const channelSchema = z.object({
  quality: z.enum(["NOT_READY", "VALID", "INVALID", "OUT_OF_RANGE", "STALE"]),
  valid: z.boolean(),
  unit: z.string(),
  source: z.enum(["simulator", "override"]),
  value: z.number().finite().nullish(),
  age_ms: z.number().nullish(),
});
export type Channel = z.infer<typeof channelSchema>;
const channelsSchema = z.object({
  temperature: channelSchema,
  optical: channelSchema,
  flow: channelSchema,
  pressure: channelSchema,
});
const telemetrySchema = z.object({
  message_kind: z.literal("telemetry"),
  simulated: z.literal(true),
  assessment_state: stateSchema,
  assessment_available: z.boolean(),
  filtered_valid: z.boolean(),
  optical_index_filtered: z.number().nullish(),
  test_override: z.boolean(),
  channels: channelsSchema,
});
export const recordSchema = z.object({
  id: z.coerce.number(),
  device_id: z.string(),
  boot_id: z.string(),
  seq: z.number(),
  received_at: z.iso.datetime({ offset: true }),
  payload: telemetrySchema,
});
export type Reading = z.infer<typeof recordSchema>;
export const eventSchema = z.object({
  contract_valid: z.boolean().default(true),
  event_id: z.string(),
  device_id: z.string(),
  received_at: z.iso.datetime({ offset: true }),
  payload: z.object({
    event_state: stateSchema,
    event_reason: z.string(),
    event_test_override: z.boolean(),
  }),
});
export type WaterEvent = z.infer<typeof eventSchema>;
export const currentSchema = z.object({
  device_id: z.string(),
  current: recordSchema.nullable(),
  communication: z.object({
    stale: z.boolean(),
    age_ms: z.number().nullable(),
    threshold_seconds: z.number(),
  }),
});
const integrationSchema = z.object({
  configured: z.boolean(),
  connected: z.boolean(),
});
export const statusSchema = z.object({
  server_time: z.iso.datetime({ offset: true }),
  devices: z.array(z.string()),
  simulated: z.literal(true),
  integrations: z.object({
    mqtt: integrationSchema,
    fiware: integrationSchema,
  }),
});
export const followupSchema = z.object({
  id: z.string(),
  event_id: z.string(),
  note: z.string(),
  status: z.enum(["OPEN", "CLOSED"]),
  created_at: z.iso.datetime({ offset: true }),
  updated_at: z.iso.datetime({ offset: true }),
});
export type Followup = z.infer<typeof followupSchema>;
export type FollowupWrite = Pick<Followup, "note" | "status">;

const configuredUrl: string =
  import.meta.env?.VITE_API_URL || "http://localhost:8000";
export const API_URL = configuredUrl.replace(/\/$/, "");

export async function request<T>(
  path: string,
  schema: z.ZodType<T>,
  options: RequestInit = {},
): Promise<T> {
  let response: Response;
  try {
    response = await fetch(`${API_URL}${path}`, {
      ...options,
      cache: "no-store",
      signal: AbortSignal.any([
        ...(options.signal ? [options.signal] : []),
        AbortSignal.timeout(90_000),
      ]),
    });
  } catch (cause: unknown) {
    if (cause instanceof DOMException && cause.name === "AbortError")
      throw cause;
    throw new Error(
      "Não foi possível conectar ao serviço. Tente novamente em alguns instantes.",
    );
  }
  if (!response.ok) {
    if (response.status === 401)
      throw new Error(
        "Chave de acesso inválida. Confira a chave administrativa.",
      );
    if (response.status === 429)
      throw new Error(
        "Muitas consultas. Aguarde um minuto para tentar novamente.",
      );
    throw new Error(
      `Não foi possível concluir a consulta (${response.status}).`,
    );
  }
  const value: unknown = await response.json();
  const parsed = schema.safeParse(value);
  if (!parsed.success)
    throw new Error("A resposta recebida tem um formato inesperado.");
  return parsed.data;
}

export function authenticated(
  token: string,
  method = "GET",
  body?: unknown,
): RequestInit {
  return {
    method,
    headers: {
      Authorization: `Bearer ${token}`,
      "Content-Type": "application/json",
    },
    ...(body === undefined ? {} : { body: JSON.stringify(body) }),
  };
}

export async function removeFollowup(id: string, token: string): Promise<void> {
  const result = await fetch(
    `${API_URL}/api/v1/followups/${encodeURIComponent(id)}`,
    authenticated(token, "DELETE"),
  );
  if (!result.ok) throw new Error("Não foi possível excluir o acompanhamento.");
}

export const readingsSchema = z.object({ items: z.array(recordSchema) });
export const eventsSchema = z.object({ items: z.array(eventSchema) });
export const followupsSchema = z.object({ items: z.array(followupSchema) });
