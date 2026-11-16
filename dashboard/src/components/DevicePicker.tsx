import { ChevronDown, RefreshCw } from "lucide-react";

interface Props {
  device: string;
  devices: string[];
  loading: boolean;
  onChange: (id: string) => void;
  onRefresh: () => void;
}

export function DevicePicker({
  device,
  devices,
  loading,
  onChange,
  onRefresh,
}: Props) {
  const options = devices.length ? devices : [device];
  return (
    <div className="device-controls">
      <div className="select-wrap">
        <label htmlFor="device">PONTO MONITORADO</label>
        <select
          id="device"
          value={device}
          onChange={(event) => onChange(event.target.value)}
        >
          {options.map((id) => (
            <option key={id} value={id}>
              {id}
            </option>
          ))}
        </select>
        <ChevronDown size={15} aria-hidden="true" />
      </div>
      <button
        className="icon-button refresh"
        type="button"
        aria-label="Atualizar dados"
        disabled={loading}
        onClick={onRefresh}
      >
        <RefreshCw
          size={18}
          className={loading ? "spinning" : ""}
          aria-hidden="true"
        />
      </button>
    </div>
  );
}
