import { Droplets, ArrowUpRight } from "lucide-react";

export function Header() {
  return (
    <header className="site-header">
      <a
        className="brand"
        href="#inicio"
        aria-label="water n’ assurance, início"
      >
        water n’
        <br />
        assurance<span className="brand-dot">.</span>
      </a>
      <nav aria-label="Navegação principal">
        <a href="#visao-geral">Visão geral</a>
        <a href="#historico">Histórico</a>
        <a href="#registros">
          Registros <ArrowUpRight size={13} aria-hidden="true" />
        </a>
      </nav>
      <div className="product-mark">
        <span>
          <Droplets size={25} strokeWidth={1.3} aria-hidden="true" />
        </span>
        <div>
          Vigia d’Água<small>UM OLHAR SOBRE A SUA ÁGUA</small>
        </div>
      </div>
    </header>
  );
}
