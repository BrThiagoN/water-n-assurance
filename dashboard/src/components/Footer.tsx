import { Droplets } from "lucide-react";

export function Footer() {
  return (
    <footer className="site-footer">
      <div className="footer-brand">
        <Droplets size={23} strokeWidth={1.1} aria-hidden="true" />
        <span>
          Informação para cuidar.
          <br />
          <strong>Clareza para agir.</strong>
        </span>
      </div>
      <div className="footer-note">
        <p>
          O Vigia d’Água acompanha alterações em parâmetros limitados. Não
          filtra a água e não substitui análises laboratoriais de potabilidade.
        </p>
        <details>
          <summary>Equipe water n’ assurance · 1ESPV-2026</summary>
          <p>
            Thiago Gomes Nascimento · RM569436
            <br />
            Rafael De Sousa · RM568777
            <br />
            Vitor Assis Florenzano · RM572192
            <br />
            Raul Moreira Andrade · RM571042
          </p>
        </details>
      </div>
      <span className="footer-year">
        PROJETO ACADÊMICO
        <br />
        2026
      </span>
    </footer>
  );
}
