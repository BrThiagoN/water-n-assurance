import { CircleAlert, Clock3, Droplet, WifiOff } from "lucide-react";
import type { Reading } from "../api.ts";
import { dateTime, stateCopy } from "../presentation.ts";

interface Props {
  reading: Reading | null;
  loading: boolean;
  stale: boolean;
  error: string | null;
}

export function StatusHero({ reading, loading, stale, error }: Props) {
  const copy = reading ? stateCopy[reading.payload.assessment_state] : null;
  return (
    <section
      className={`status-hero ${copy?.tone ?? "waiting"}`}
      aria-labelledby="status-title"
    >
      <div className="hero-copy">
        <div className="hero-kicker">
          <span className="status-dot" />{" "}
          {reading
            ? stale
              ? "ÚLTIMO ESTADO RECEBIDO"
              : "ESTADO INFORMADO PELO APARELHO"
            : "UM NOVO JEITO DE ACOMPANHAR"}
        </div>
        <h2 id="status-title" aria-live="polite">
          {copy?.title ??
            (loading
              ? "Buscando a sua\nprimeira leitura."
              : "Aguardando a\nprimeira leitura.")}
        </h2>
        <p>
          {copy?.description ??
            (error
              ? "Não conseguimos consultar o aparelho agora. Tente atualizar em alguns instantes."
              : "Assim que o aparelho enviar os dados, você verá aqui o que mudou. De um jeito simples, com um olhar.")}
        </p>
        <div className="hero-meta">
          {reading ? (
            <>
              <Clock3 size={14} aria-hidden="true" /> Recebido em{" "}
              {dateTime(reading.received_at)} · horário de Brasília
            </>
          ) : (
            <>
              <Droplet size={14} aria-hidden="true" /> Seu histórico começa com
              a primeira leitura
            </>
          )}
        </div>
      </div>
      <div className="water-art" aria-hidden="true">
        <div className="orbit orbit-one" />
        <div className="orbit orbit-two" />
        <div className="orbit orbit-three" />
        <div className="water-core">
          <Droplet size={64} strokeWidth={0.8} />
        </div>
        <span>VIGIA D’ÁGUA</span>
      </div>
      {reading && stale && (
        <div className="stale-note" role="status">
          <WifiOff size={16} aria-hidden="true" /> Sem atualização recente. Os
          valores anteriores não representam uma leitura atual.
        </div>
      )}
      {reading && !stale && reading.payload.assessment_state === "CHECK" && (
        <div className="stale-note">
          <CircleAlert size={16} aria-hidden="true" /> O aviso indica alteração
          nos parâmetros acompanhados; a causa precisa ser investigada.
        </div>
      )}
    </section>
  );
}
