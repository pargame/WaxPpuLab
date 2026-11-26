"""공격 에셋 헤드리스 검증.

실행:
  UnrealEditor-Cmd WaxPpuLab.uproject -run=pythonscript -script=Scripts/validate_attacks.py -unattended -nullrhi
결과는 'WAXVAL PASS|FAIL ...' 로그 줄로 남고, FAIL이 하나라도 있으면 예외로 종료 코드가 0이 아니게 된다.
"""
import unreal

AL = unreal.AnimationLibrary
PLAYER_BPS = ['/Game/WaxPpu/Player/BP_WaxPlayer', '/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter']
ATTACK_INPUT = '/Game/Input/Actions/IA_WaxCrack'

failures = []


def report(ok, msg):
    unreal.log_warning(('WAXVAL PASS ' if ok else 'WAXVAL FAIL ') + msg)
    if not ok:
        failures.append(msg)


def montage_hits(montage):
    """몽타주 및 내부 시퀀스의 WaxAttackHitNotify (몽타주 시간, 본 이름)."""
    hits = []
    for e in AL.get_animation_notify_events(montage):
        n = e.get_editor_property('notify')
        if isinstance(n, unreal.WaxAttackHitNotify):
            hits.append((AL.get_anim_notify_event_trigger_time(e), n.get_editor_property('impact_bone_name')))
    for track in montage.get_editor_property('slot_anim_tracks'):
        for seg in track.get_editor_property('anim_track').get_editor_property('anim_segments'):
            seq = seg.get_editor_property('anim_reference')
            if not seq:
                continue
            start, a0, a1 = seg.get_editor_property('start_pos'), seg.get_editor_property('anim_start_time'), seg.get_editor_property('anim_end_time')
            rate = max(abs(seg.get_editor_property('anim_play_rate')), 1e-4)
            for e in AL.get_animation_notify_events(seq):
                n = e.get_editor_property('notify')
                t = AL.get_anim_notify_event_trigger_time(e)
                if isinstance(n, unreal.WaxAttackHitNotify) and a0 <= t <= a1:
                    hits.append((start + (t - a0) / rate, n.get_editor_property('impact_bone_name')))
    return hits


def state_windows(montage, cls):
    out = []
    for e in AL.get_animation_notify_events(montage):
        n = e.get_editor_property('notify_state_class')
        if n and isinstance(n, cls):
            t = AL.get_anim_notify_event_trigger_time(e)
            out.append((t, t + AL.get_anim_notify_event_duration(e), n))
    return out


def validate_spec(label, spec, body_mesh, required):
    montages = [m for m in spec.get_editor_property('montages')]
    if not montages:
        report(not required, f'{label}: montages empty' + ('' if required else ' (not yet authored)'))
        return
    report(all(montages), f'{label}: all montages non-null ({len(montages)})')
    report(spec.get_editor_property('play_rate') > 0, f'{label}: play_rate > 0')
    for m in montages:
        if not m:
            continue
        name = f'{label}/{m.get_name()}'
        length = m.get_play_length()
        hits = montage_hits(m)
        report(len(hits) >= 1, f'{name}: hit notify count={len(hits)}')
        for t, bone in hits:
            report(0.0 <= t <= length, f'{name}: hit t={t:.3f} within [0,{length:.3f}]')
            report(body_mesh.does_socket_exist(bone), f'{name}: bone/socket "{bone}" exists on body mesh')
        actives = state_windows(m, unreal.WaxAttackActiveNotifyState)
        report(len(actives) <= 1, f'{name}: at most one Active window ({len(actives)})')
        for a0, a1, _ in actives:
            covers = any(a0 - 1e-3 <= t <= a1 + 1e-3 for t, _ in hits)
            report(covers, f'{name}: Active [{a0:.3f},{a1:.3f}] covers a hit')
        for c0, c1, n in state_windows(m, unreal.WaxCancelWindowNotifyState):
            slots = list(n.get_editor_property('allowed_slots'))
            report(len(slots) > 0, f'{name}: CancelWindow [{c0:.3f},{c1:.3f}] allowed={[str(s) for s in slots]}')
            last_hit = max([t for t, _ in hits], default=0.0)
            report(c0 >= last_hit - 1e-3, f'{name}: CancelWindow opens after last hit ({c0:.3f} >= {last_hit:.3f})')
        slots = [str(t.get_editor_property('slot_name')) for t in m.get_editor_property('slot_anim_tracks')]
        report(len(slots) == 1, f'{name}: single slot track {slots}')


def main():
    attack_sets = set()
    for bp_path in PLAYER_BPS:
        bp = unreal.load_asset(bp_path)
        report(bp is not None, f'{bp_path}: loads')
        if not bp:
            continue
        cdo = unreal.get_default_object(bp.generated_class())
        comp = cdo.get_editor_property('combat_component')
        attack_set = comp.get_editor_property('attack_set')
        report(attack_set is not None, f'{bp_path}: CombatComponent.AttackSet assigned')
        report(comp.get_editor_property('targeting_mpc') is not None, f'{bp_path}: CombatComponent.TargetingMPC assigned')
        tick_opt = cdo.get_editor_property('mesh').get_editor_property('visibility_based_anim_tick_option')
        report(tick_opt == unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES,
               f'{bp_path}: body mesh refreshes bones when hidden ({tick_opt})')
        if attack_set:
            attack_sets.add((attack_set.get_path_name(), bp_path))

    for da_path, bp_path in sorted(attack_sets):
        da = unreal.load_asset(da_path)
        cdo = unreal.get_default_object(unreal.load_asset(bp_path).generated_class())
        body = cdo.get_editor_property('mesh')
        validate_spec('Basic', da.get_editor_property('basic'), body, True)
        jump = da.get_editor_property('jump_attack')
        validate_spec('AerialStrike', jump.get_editor_property('aerial_strike'), body, False)
        validate_spec('Slam', jump.get_editor_property('slam'), body, False)
        validate_spec('Kick', da.get_editor_property('kick'), body, False)
        st = da.get_editor_property('settings')
        report(st.get_editor_property('buffer_window_sec') > 0, f'{da.get_name()}: buffer_window_sec > 0')

    ia = unreal.load_asset(ATTACK_INPUT)
    triggers = [t.get_class().get_name() for t in ia.get_editor_property('triggers')]
    report('InputTriggerPulse' not in triggers, f'IA_WaxCrack: no Pulse trigger {triggers}')

    unreal.log_warning(f'WAXVAL SUMMARY failures={len(failures)}')
    if failures:
        raise RuntimeError(f'{len(failures)} validation failure(s)')


main()
