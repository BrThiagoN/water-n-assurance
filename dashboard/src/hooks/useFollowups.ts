import { useCallback, useEffect, useState } from "react";
import {
  authenticated,
  followupSchema,
  followupsSchema,
  removeFollowup,
  request,
} from "../api.ts";
import type { Followup, FollowupWrite } from "../api.ts";

export function useFollowups(token: string): {
  items: Followup[];
  error: string | null;
  busy: boolean;
  create: (eventId: string, note: string) => Promise<boolean>;
  update: (id: string, data: FollowupWrite) => Promise<boolean>;
  remove: (id: string) => Promise<boolean>;
} {
  const [items, setItems] = useState<Followup[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const reload = useCallback(
    async (signal?: AbortSignal): Promise<void> => {
      const result = await request("/api/v1/followups", followupsSchema, {
        ...authenticated(token),
        ...(signal ? { signal } : {}),
      });
      setItems(result.items);
    },
    [token],
  );
  useEffect(() => {
    const controller = new AbortController();
    setItems([]);
    setError(null);
    if (token)
      void reload(controller.signal).catch((cause: unknown) => {
        if (!controller.signal.aborted)
          setError(
            cause instanceof Error ? cause.message : "Consulta indisponível.",
          );
      });
    return () => {
      controller.abort();
    };
  }, [token, reload]);
  const act = async (operation: () => Promise<unknown>): Promise<boolean> => {
    setBusy(true);
    setError(null);
    try {
      await operation();
      await reload();
      return true;
    } catch (cause: unknown) {
      setError(
        cause instanceof Error ? cause.message : "Operação não concluída.",
      );
      return false;
    } finally {
      setBusy(false);
    }
  };
  return {
    items,
    error,
    busy,
    create: (eventId, note) =>
      act(() =>
        request(
          "/api/v1/followups",
          followupSchema,
          authenticated(token, "POST", {
            event_id: eventId,
            note,
            status: "OPEN",
          }),
        ),
      ),
    update: (id, data) =>
      act(() =>
        request(
          `/api/v1/followups/${encodeURIComponent(id)}`,
          followupSchema,
          authenticated(token, "PATCH", data),
        ),
      ),
    remove: (id) => act(() => removeFollowup(id, token)),
  };
}
