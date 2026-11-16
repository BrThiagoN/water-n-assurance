import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "@fontsource/dm-serif-display/latin-400.css";
import "@fontsource/source-sans-3/latin-400.css";
import "@fontsource/source-sans-3/latin-600.css";
import "@fontsource/source-sans-3/latin-700.css";
import "./styles.css";
import { App } from "./App.tsx";

const root = document.getElementById("root");
if (!root) throw new Error("Elemento raiz não encontrado.");
createRoot(root).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
