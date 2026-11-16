import { useEffect, useState } from "react";
import { Activity, Droplets, Gauge, Thermometer } from "lucide-react";
import { DevicePicker } from "./components/DevicePicker.tsx";
import { EventsList } from "./components/EventsList.tsx";
import { FollowupsPanel } from "./components/FollowupsPanel.tsx";
import { Footer } from "./components/Footer.tsx";
import { Header } from "./components/Header.tsx";
import { MetricCard } from "./components/MetricCard.tsx";
import { ReadingsChart } from "./components/ReadingsChart.tsx";
import { StatusHero } from "./components/StatusHero.tsx";
import { useMonitor } from "./hooks/useMonitor.ts";
import { isStale } from "./presentation.ts";

export function App() {
  const [device, setDevice] = useState("vigia-sim-01");
  const [selectedEvent, setSelectedEvent] = useState("");
  const [now, setNow] = useState(Date.now());
  const { data, loading, error, refresh } = useMonitor(device);
  useEffect(() => {
    const timer = window.setInterval(() => setNow(Date.now()), 1000);
    return () => window.clearInterval(timer);
  }, []);
  const reading = data?.current.current ?? null;
  const stale = isStale(
    reading?.received_at,
    data?.current.communication.threshold_seconds ?? 20,
    now,
  );
  const selectEvent = (id: string): void => {
    setSelectedEvent(id);
    document
      .getElementById("acompanhamentos")
      ?.scrollIntoView({ behavior: "smooth", block: "start" });
  };
  return (
    <div id="inicio" className="page-shell">
      <a className="skip-link" href="#visao-geral">
        Ir para o conteúdo
      </a>
      <Header />
      <main id="visao-geral">
        <div className="page-intro">
          <div>
            <span className="eyebrow">
              ÁGUA EM CASA. INFORMAÇÃO NAS SUAS MÃOS.
            </span>
            <h1>Um olhar mais atento.</h1>
            <p>
              Perceba mudanças. Consulte o histórico. Saiba por onde começar.
            </p>
          </div>
          <DevicePicker
            device={device}
            devices={data?.status.devices ?? []}
            loading={loading}
            onChange={(id) => {
              setDevice(id);
              setSelectedEvent("");
            }}
            onRefresh={refresh}
          />
        </div>
        <div className="demo-strip">
          <span className="small-badge">DEMONSTRAÇÃO</span>
          <p>
            Leituras simuladas para explorar o funcionamento do Vigia d’Água.
          </p>
          <span className={`connection-label ${error ? "offline" : ""}`}>
            <span />
            {loading
              ? "Consultando"
              : error
                ? "Conexão indisponível"
                : reading && !stale
                  ? "Dados atualizados"
                  : "Aguardando dados"}
          </span>
        </div>
        {error && (
          <div className="inline-error" role="alert">
            {error} O serviço pode levar alguns instantes para responder.{" "}
            <button className="text-button" onClick={refresh} type="button">
              Tentar novamente
            </button>
          </div>
        )}
        <StatusHero
          reading={reading}
          loading={loading}
          stale={stale}
          error={error}
        />
        <div className="metrics-grid">
          <MetricCard
            title="Índice óptico"
            unit="/ 100"
            caption="Variações na resposta óptica"
            icon={Droplets}
            channel={reading?.payload.channels.optical}
            stale={stale}
          />
          <MetricCard
            title="Temperatura"
            unit="°C"
            caption="Contexto da leitura"
            icon={Thermometer}
            channel={reading?.payload.channels.temperature}
            stale={stale}
          />
          <MetricCard
            title="Vazão"
            unit="L/min"
            caption="Circulação no ponto monitorado"
            icon={Activity}
            channel={reading?.payload.channels.flow}
            stale={stale}
          />
          <MetricCard
            title="Pressão"
            unit="bar"
            caption="Condições do abastecimento"
            icon={Gauge}
            channel={reading?.payload.channels.pressure}
            stale={stale}
          />
        </div>
        <ReadingsChart
          readings={data?.readings ?? []}
          device={device}
          now={now}
        />
        <EventsList events={data?.events ?? []} onSelect={selectEvent} />
        <FollowupsPanel
          selected={selectedEvent}
          events={data?.events ?? []}
          onSelect={setSelectedEvent}
          device={device}
        />
      </main>
      <Footer />
    </div>
  );
}
