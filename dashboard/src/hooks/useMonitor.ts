import { useEffect, useState } from "react";
import {
  currentSchema,
  eventsSchema,
  readingsSchema,
  request,
  statusSchema,
} from "../api.ts";
import type { z } from "zod";

export interface MonitorData {
  current: z.infer<typeof currentSchema>;
  currentObservedAt: number;
  statusObservedAt: number;
  readings: z.infer<typeof readingsSchema>["items"];
  events: z.infer<typeof eventsSchema>["items"];
  status: z.infer<typeof statusSchema>;
}

export function useMonitor(device: string): {
  data: MonitorData | null;
  loading: boolean;
  error: string | null;
  refresh: () => void;
} {
  const [data, setData] = useState<MonitorData | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [revision, setRevision] = useState(0);
  useEffect(() => {
    const controller = new AbortController();
    let busy = false;
    setLoading(true);
    const load = async (): Promise<void> => {
      if (busy) return;
      busy = true;
      try {
        const path = `/api/v1/devices/${encodeURIComponent(device)}`;
        const options = { signal: controller.signal };
        const [current, readings, events, status] = await Promise.all([
          request(`${path}/current`, currentSchema, options).then((result) => ({
            result,
            observedAt: performance.now(),
          })),
          request(`${path}/telemetry?limit=200`, readingsSchema, options),
          request(`${path}/events?limit=20`, eventsSchema, options),
          request("/api/v1/status", statusSchema, options).then((result) => ({
            result,
            observedAt: performance.now(),
          })),
        ]);
        if (!controller.signal.aborted) {
          setData({
            current: current.result,
            currentObservedAt: current.observedAt,
            statusObservedAt: status.observedAt,
            readings: readings.items,
            events: events.items,
            status: status.result,
          });
          setError(null);
        }
      } catch (cause: unknown) {
        if (!controller.signal.aborted)
          setError(
            cause instanceof Error ? cause.message : "Conexão indisponível.",
          );
      } finally {
        busy = false;
        if (!controller.signal.aborted) setLoading(false);
      }
    };
    void load();
    const timer = window.setInterval(() => {
      void load();
    }, 10_000);
    return () => {
      controller.abort();
      window.clearInterval(timer);
    };
  }, [device, revision]);
  return {
    data: data?.current.device_id === device ? data : null,
    loading: loading || (data !== null && data.current.device_id !== device),
    error,
    refresh: () => {
      setRevision((value) => value + 1);
    },
  };
}
