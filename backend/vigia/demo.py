"""Mensagens sintéticas para smoke test; não são medições de água."""

from vigia.models import Channel, Channels, Event, State, Telemetry


def sample(boot_id: str = "demo0001", seq: int = 10, state: State = "NO_CHANGE") -> Telemetry:
    index = 80.0 if state == "CHECK" else 40.0 if state == "ATTENTION" else 20.0
    return Telemetry(
        schema_version=2,
        rule_version="median5-raw3-v2",
        message_kind="telemetry",
        device_id="vigia-sim-01",
        boot_id=boot_id,
        seq=seq,
        uptime_ms=20_000,
        sent_uptime_ms=20_000,
        time_synced=False,
        simulated=True,
        test_override=True,
        assessment_available=state != "UNAVAILABLE",
        assessment_scope="relative_optical_change",
        optical_observation="NONE",
        observation_provisional=False,
        readings_valid=True,
        filtered_valid=True,
        assessment_state=state,
        reason="ASSESSED",
        flow_context="FLOWING",
        last_alert_status="CLEARED" if state == "NO_CHANGE" else "ACTIVE",
        last_alert_state="NONE" if state == "NO_CHANGE" else "CHECK",
        dropped_events=0,
        temperature_c=25.0,
        optical_index_raw=index,
        optical_index_filtered=index,
        flow_l_min=6.0,
        pressure_bar=2.0,
        channels=Channels(
            temperature=Channel(
                quality="VALID", valid=True, unit="degC", source="simulator", value=25.0, age_ms=250
            ),
            optical=Channel(
                quality="VALID",
                valid=True,
                unit="relative_index",
                source="override",
                value=index,
                age_ms=0,
            ),
            flow=Channel(
                quality="VALID", valid=True, unit="L/min", source="override", value=6.0, age_ms=0
            ),
            pressure=Channel(
                quality="VALID", valid=True, unit="bar", source="simulator", value=2.0, age_ms=0
            ),
        ),
    )


def event(boot_id: str = "demo0001", seq: int = 1) -> Event:
    snapshot = sample(boot_id, state="CHECK")
    return Event(
        schema_version=2,
        message_kind="event",
        device_id=snapshot.device_id,
        boot_id=boot_id,
        time_synced=False,
        simulated=True,
        event_rule_version=snapshot.rule_version,
        event_id=f"{snapshot.device_id}:{boot_id}:{seq}",
        event_seq=seq,
        event_sample_seq=10,
        event_uptime_ms=20_000,
        sent_uptime_ms=21_000,
        event_state="CHECK",
        event_reason="ASSESSED",
        event_flow_context="FLOWING",
        event_assessment_available=True,
        event_test_override=True,
        event_readings_valid=True,
        event_filtered_valid=True,
        event_channels=snapshot.channels,
        event_temperature_c=25.0,
        event_optical_index_raw=80.0,
        event_optical_index_filtered=80.0,
        event_flow_l_min=6.0,
        event_pressure_bar=2.0,
        event_assessment_scope="relative_optical_change",
        event_optical_observation="NONE",
        event_observation_provisional=False,
        event_last_alert_status="ACTIVE",
        event_last_alert_state="CHECK",
        dropped_events=0,
    )
