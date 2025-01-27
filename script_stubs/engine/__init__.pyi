from enum import Enum, auto
from typing import Optional, Dict, Set


class ObjectInfo:
    def __init__(self):
        ...

    ai_agent: bool
    radius: int
    hit_points: int
    pivot_length: int
    target_update_chance: int

    drop_limit: int
    step_limit: int
    fly_limit: int
    cannot_visit_blocked: bool
    cannot_visit_blockable: bool


class ActivationState(Enum):
    INACTIVE = auto()
    DEACTIVATED = auto()
    INVISIBLE = auto()


class TrackType(Enum):
    AMBIENT = auto()
    INTERCEPTION = auto()
    AMBIENT_EFFECT = auto()


class WeaponType(Enum):
    None_ = auto()
    Pistols = auto()
    Magnums = auto()
    Uzis = auto()
    Shotgun = auto()


class TR1SoundEffect(Enum):
    ...


class TR1TrackId(Enum):
    ...


class TR1ItemId(Enum):
    ...


class TrackInfo:
    def __init__(self, soundid: int, tracktype: TrackType, /):
        ...


class LevelSequenceItem:
    ...


class Level(LevelSequenceItem):
    def __init__(
            self, *,
            name: str,
            secrets: int,
            titles: Dict[str, str],
            track: Optional[TR1TrackId] = None,
            item_titles: Dict[str, Dict[TR1ItemId, str]] = {},
            inventory: Dict[TR1ItemId, int] = {},
            drop_inventory: Set[TR1ItemId] = set(),
            use_alternative_lara: bool = False,
            allow_save: bool = True,
            default_weapon: WeaponType = WeaponType.Pistols,
    ):
        ...


class TitleMenu(Level):
    ...


class Video(LevelSequenceItem):
    def __init__(self, name: str):
        ...


class Cutscene(LevelSequenceItem):
    def __init__(
            self, *,
            name: str,
            track: TR1TrackId,
            camera_rot: float,
            weapon_swap: bool = False,
            flip_rooms: bool = False,
            camera_pos_x: Optional[int] = None,
            camera_pos_z: Optional[int] = None,
    ):
        ...


class SplashScreen(LevelSequenceItem):
    def __init__(
            self, *,
            path: str,
            duration_seconds: int,
    ):
        ...
