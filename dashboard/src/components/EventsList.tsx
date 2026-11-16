import { ArrowUpRight, ListChecks } from "lucide-react";
import type { WaterEvent } from "../api.ts";
import { dateTime, stateCopy } from "../presentation.ts";

interface Props {
  events: WaterEvent[];
  onSelect: (id: string) => void;
}

export function EventsList({ events, onSelect }: Props) {
  return (
    <section
      id="registros"
      className="events-panel"
      aria-labelledby="events-title"
    >
      <div className="section-heading">
        <div>
          <span className="eyebrow">O QUE O APARELHO PERCEBEU</span>
          <h2 id="events-title">Um registro para agir.</h2>
        </div>
        <span className="count-label">
          {events.length} {events.length === 1 ? "ocorrência" : "ocorrências"}
        </span>
      </div>
      {events.length ? (
        <ol className="events-list">
          {events.slice(0, 8).map((event) => (
            <li key={event.event_id}>
              <span
                className={`event-mark ${stateCopy[event.payload.event_state].tone}`}
              />
              <div>
                <h3>{stateCopy[event.payload.event_state].title}</h3>
                <p>
                  {dateTime(event.received_at)}
                  {event.payload.event_test_override && " · entrada de teste"}
                  {!event.contract_valid &&
                    " · registro antigo com dados incompletos"}
                </p>
              </div>
              <button
                type="button"
                className="text-button"
                onClick={() => onSelect(event.event_id)}
              >
                Acompanhar <ArrowUpRight size={15} aria-hidden="true" />
              </button>
            </li>
          ))}
        </ol>
      ) : (
        <div className="list-empty">
          <ListChecks size={26} strokeWidth={1.2} aria-hidden="true" />
          <div>
            <h3>Nenhuma ocorrência recebida.</h3>
            <p>
              As mudanças de estado enviadas pelo aparelho ficam registradas
              aqui.
            </p>
          </div>
        </div>
      )}
    </section>
  );
}
