import { useEffect, useState } from "react";
import { ArrowRight, Check, KeyRound, Pencil, Trash2, X } from "lucide-react";
import type { FormEvent } from "react";
import type { WaterEvent } from "../api.ts";
import { useFollowups } from "../hooks/useFollowups.ts";
import { dateTime, stateCopy } from "../presentation.ts";

interface Props {
  selected: string;
  events: WaterEvent[];
  onSelect: (id: string) => void;
  device: string;
}

export function FollowupsPanel({ selected, events, onSelect, device }: Props) {
  const [open, setOpen] = useState(false);
  const [key, setKey] = useState("");
  const [token, setToken] = useState("");
  const [note, setNote] = useState("");
  const [editing, setEditing] = useState<string | null>(null);
  const [editedNote, setEditedNote] = useState("");
  const [confirmDelete, setConfirmDelete] = useState<string | null>(null);
  const { items, error, busy, create, update, remove } = useFollowups(token);
  useEffect(() => {
    if (selected) setOpen(true);
  }, [selected]);
  const submit = async (event: FormEvent<HTMLFormElement>): Promise<void> => {
    event.preventDefault();
    if (await create(selected, note.trim())) setNote("");
  };
  return (
    <section
      className="followups-panel"
      id="acompanhamentos"
      aria-labelledby="followups-title"
    >
      <div className="section-heading">
        <div>
          <span className="eyebrow">DO REGISTRO À PROVIDÊNCIA</span>
          <h2 id="followups-title">O próximo passo, anotado.</h2>
        </div>
        <button
          type="button"
          className="text-button"
          aria-expanded={open}
          onClick={() => setOpen((value) => !value)}
        >
          {open ? "Recolher" : "Abrir acompanhamentos"}{" "}
          <ArrowRight size={16} aria-hidden="true" />
        </button>
      </div>
      <p className="muted">
        Guarde o que foi verificado, solicitado ou resolvido para cada
        ocorrência.
      </p>
      {open && (
        <div className="followups-content">
          {!token ? (
            <form
              className="access-form"
              onSubmit={(event) => {
                event.preventDefault();
                setToken(key.trim());
                setKey("");
              }}
            >
              <KeyRound size={20} strokeWidth={1.4} aria-hidden="true" />
              <div>
                <label htmlFor="admin-key">Acesso da equipe responsável</label>
                <p>
                  A chave permite registrar e editar providências. Ela fica
                  apenas nesta sessão da página.
                </p>
              </div>
              <input
                id="admin-key"
                type="password"
                autoComplete="off"
                placeholder="Chave administrativa"
                value={key}
                onChange={(event) => setKey(event.target.value)}
                required
                minLength={32}
              />
              <button className="primary-button" type="submit">
                Acessar <ArrowRight size={16} aria-hidden="true" />
              </button>
            </form>
          ) : (
            <>
              <div className="access-note">
                <Check size={15} aria-hidden="true" /> Chave informada nesta
                sessão{" "}
                <button
                  className="text-button"
                  type="button"
                  onClick={() => {
                    setToken("");
                    setKey("");
                  }}
                >
                  Sair
                </button>
              </div>
              <form
                className="followup-form"
                onSubmit={(event) => {
                  void submit(event);
                }}
              >
                <label htmlFor="occurrence">Ocorrência</label>
                <select
                  id="occurrence"
                  value={selected}
                  onChange={(event) => onSelect(event.target.value)}
                  required
                >
                  <option value="">Escolha uma ocorrência</option>
                  {events.map((event) => (
                    <option key={event.event_id} value={event.event_id}>
                      {dateTime(event.received_at)} ·{" "}
                      {stateCopy[event.payload.event_state].title}
                    </option>
                  ))}
                </select>
                <label htmlFor="note">Providência ou observação</label>
                <textarea
                  id="note"
                  placeholder="Ex.: solicitei uma verificação do ponto monitorado."
                  value={note}
                  onChange={(event) => setNote(event.target.value)}
                  maxLength={1000}
                  required
                  rows={3}
                />
                <button
                  className="primary-button"
                  type="submit"
                  disabled={busy || !selected || !note.trim()}
                >
                  Registrar acompanhamento{" "}
                  <ArrowRight size={16} aria-hidden="true" />
                </button>
              </form>
              <div className="followup-list">
                {items
                  .filter((item) => item.event_id.startsWith(`${device}:`))
                  .map((item) => (
                    <article key={item.id} className="followup-item">
                      <div className="followup-meta">
                        <span
                          className={`small-badge ${item.status === "CLOSED" ? "closed" : ""}`}
                        >
                          {item.status === "OPEN"
                            ? "Em acompanhamento"
                            : "Concluído"}
                        </span>
                        <time dateTime={item.updated_at}>
                          {dateTime(item.updated_at)}
                        </time>
                      </div>
                      {editing === item.id ? (
                        <form
                          onSubmit={(event) => {
                            event.preventDefault();
                            void update(item.id, {
                              note: editedNote.trim(),
                              status: item.status,
                            }).then((saved) => {
                              if (saved) setEditing(null);
                            });
                          }}
                        >
                          <label htmlFor={`edit-${item.id}`}>
                            Editar observação
                          </label>
                          <textarea
                            id={`edit-${item.id}`}
                            value={editedNote}
                            maxLength={1000}
                            rows={3}
                            required
                            onChange={(event) =>
                              setEditedNote(event.target.value)
                            }
                          />
                          <div className="item-actions">
                            <button
                              className="text-button"
                              type="submit"
                              disabled={busy || !editedNote.trim()}
                            >
                              Salvar
                            </button>
                            <button
                              type="button"
                              className="text-button"
                              onClick={() => setEditing(null)}
                            >
                              Cancelar
                            </button>
                          </div>
                        </form>
                      ) : (
                        <p>{item.note}</p>
                      )}
                      <div className="item-actions">
                        <button
                          className="text-button"
                          disabled={busy}
                          type="button"
                          onClick={() => {
                            void update(item.id, {
                              note: item.note,
                              status:
                                item.status === "OPEN" ? "CLOSED" : "OPEN",
                            });
                          }}
                        >
                          <Check size={14} aria-hidden="true" />
                          {item.status === "OPEN"
                            ? "Marcar como concluído"
                            : "Reabrir"}
                        </button>
                        <button
                          className="text-button"
                          type="button"
                          onClick={() => {
                            setEditing(item.id);
                            setEditedNote(item.note);
                          }}
                        >
                          <Pencil size={14} aria-hidden="true" /> Editar
                        </button>
                        <button
                          className="text-button"
                          type="button"
                          onClick={() => setConfirmDelete(item.id)}
                        >
                          <Trash2 size={14} aria-hidden="true" /> Excluir
                        </button>
                      </div>
                      {confirmDelete === item.id && (
                        <div
                          className="delete-confirm"
                          role="group"
                          aria-label="Confirmar exclusão"
                        >
                          <p>
                            Excluir esta anotação? O evento original será
                            preservado.
                          </p>
                          <button
                            className="text-button"
                            type="button"
                            disabled={busy}
                            onClick={() => {
                              void remove(item.id).then((removed) => {
                                if (removed) setConfirmDelete(null);
                              });
                            }}
                          >
                            Confirmar exclusão
                          </button>
                          <button
                            className="icon-button"
                            aria-label="Cancelar exclusão"
                            type="button"
                            onClick={() => setConfirmDelete(null)}
                          >
                            <X size={16} aria-hidden="true" />
                          </button>
                        </div>
                      )}
                    </article>
                  ))}
              </div>
            </>
          )}
          {error && (
            <div className="inline-error" role="alert">
              {error}
              {token && (
                <button
                  className="text-button"
                  type="button"
                  onClick={() => setToken("")}
                >
                  Informar outra chave
                </button>
              )}
            </div>
          )}
        </div>
      )}
    </section>
  );
}
