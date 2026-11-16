import { useMemo, useState } from "react";
import { ArrowDownToLine, Waves } from "lucide-react";
import { API_URL } from "../api.ts";
import type { Reading } from "../api.ts";
import { chartSegments, clockTime } from "../presentation.ts";

interface Props {
  readings: Reading[];
  device: string;
  now: number;
}

export function ReadingsChart({ readings, device, now }: Props) {
  const [period, setPeriod] = useState("all");
  const filtered = useMemo(
    () =>
      readings.filter(
        (row) =>
          period === "all" ||
          Date.parse(row.received_at) >=
            now - (period === "hour" ? 3_600_000 : 86_400_000),
      ),
    [readings, period, now],
  );
  const segments = useMemo(() => chartSegments(filtered), [filtered]);
  const oldest = [...filtered].sort(
    (a, b) => Date.parse(a.received_at) - Date.parse(b.received_at),
  )[0];
  const newest = [...filtered].sort(
    (a, b) => Date.parse(b.received_at) - Date.parse(a.received_at),
  )[0];
  return (
    <section
      className="chart-panel"
      id="historico"
      aria-labelledby="chart-title"
    >
      <div className="section-heading">
        <div>
          <span className="eyebrow">ACOMPANHE AO LONGO DO TEMPO</span>
          <h2 id="chart-title">Cada mudança conta.</h2>
        </div>
        <select
          aria-label="Período do histórico"
          value={period}
          onChange={(event) => setPeriod(event.target.value)}
        >
          <option value="all">Últimos registros</option>
          <option value="hour">Última hora</option>
          <option value="day">Últimas 24 horas</option>
        </select>
      </div>
      <div className="chart-legend">
        <span />
        <p>
          Índice óptico relativo{" "}
          <small>escala de 0 a 100 · sem unidade de potabilidade</small>
        </p>
      </div>
      {segments.length ? (
        <svg
          viewBox="0 0 950 230"
          className="history-chart"
          role="img"
          aria-label={`Histórico do índice óptico relativo com ${filtered.length} registros. Leituras inválidas interrompem a linha.`}
        >
          {[0, 50, 100].map((value) => (
            <g key={value}>
              <line
                x1="50"
                x2="900"
                y1={190 - value * 1.6}
                y2={190 - value * 1.6}
                className="chart-grid"
              />
              <text x="15" y={194 - value * 1.6}>
                {value}
              </text>
            </g>
          ))}
          {segments.map((segment, index) => (
            <g key={index}>
              <polyline points={segment.points} className="chart-line" />
              {segment.points.split(" ").map((point, pointIndex) => {
                const [x, y] = point.split(",");
                return (
                  <circle
                    key={pointIndex}
                    cx={x}
                    cy={y}
                    r="3"
                    className="chart-point"
                  />
                );
              })}
            </g>
          ))}
          {oldest && (
            <text x="50" y="220">
              {clockTime(oldest.received_at)}
            </text>
          )}
          {newest && (
            <text x="900" y="220" textAnchor="end">
              {clockTime(newest.received_at)}
            </text>
          )}
        </svg>
      ) : (
        <div className="chart-empty">
          <Waves size={34} strokeWidth={1} aria-hidden="true" />
          <p>Ainda não há leituras válidas neste período.</p>
          <span>
            Seu histórico aparecerá aqui, sem preencher o que ainda não foi
            medido.
          </span>
        </div>
      )}
      <div className="chart-footer">
        <span>Até 200 leituras · horário de recebimento em Brasília</span>
        <a
          href={`${API_URL}/api/v1/devices/${encodeURIComponent(device)}/export.csv`}
          download
        >
          <ArrowDownToLine size={15} aria-hidden="true" /> Exportar registros
        </a>
      </div>
    </section>
  );
}
