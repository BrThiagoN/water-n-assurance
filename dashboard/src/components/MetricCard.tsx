import type { LucideIcon } from "lucide-react";
import type { Channel } from "../api.ts";
import { number, visibleValue } from "../presentation.ts";

interface Props {
  title: string;
  unit: string;
  caption: string;
  icon: LucideIcon;
  channel: Channel | undefined;
  stale: boolean;
}

export function MetricCard({
  title,
  unit,
  caption,
  icon: Icon,
  channel,
  stale,
}: Props) {
  const usable = channel?.valid && typeof channel.value === "number";
  const detail =
    stale && usable && typeof channel.value === "number"
      ? `Último valor recebido: ${number(channel.value)} ${unit}`
      : !usable
        ? "Aguardando leitura válida"
        : channel.source === "override"
          ? "Entrada de teste"
          : "Entrada da simulação";
  return (
    <article className="metric-card">
      <div className="metric-top">
        <h3>{title}</h3>
        <Icon size={20} strokeWidth={1.4} aria-hidden="true" />
      </div>
      <div className="metric-value">
        {visibleValue(channel, stale)}
        <span>{unit}</span>
      </div>
      <p>{caption}</p>
      <small>{detail}</small>
    </article>
  );
}
