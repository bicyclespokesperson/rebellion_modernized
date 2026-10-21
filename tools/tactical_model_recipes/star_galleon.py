"""Star Galleon's armoured freight shell, narrow command prow and inset cargo hatches."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 26
NAME = "Star Galleon"
ENCYCLOPEDIA = "EDATA.069"
LENGTH = 927.0
VIEW = {"yaw": (30, 85), "pitch": (0, 30)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("star_galleon_hull",palette.light,27,contrast=.11,seam=.66)
    panel = kit.plated("star_galleon_panels",kit.mix(palette.light,palette.mid,.28),18,contrast=.08)
    dark = kit.plain("star_galleon_recess",kit.mix(palette.dark,palette.mid,.1),metallic=.3)
    trim = kit.plain("star_galleon_trim",kit.mix(palette.mid,palette.light,.4),metallic=.4)
    exhaust = kit.emissive("star_galleon_exhaust",(.15,.4,1,1),3)
    roof = [(-463,106,55),(-402,63,99),(-102,106,138),(134,106,110),(254,106,41),(463,81,-1)]
    shoulders = [(-463,106,55),(-402,118,59),(-91,178,78),(132,164,67),(254,106,41),(463,81,-1)]
    lower = [(-463,106,55),(-381,178,-75),(-49,178,-77),(134,178,-82),(241,74,-129),(429,57,-84)]
    keel = [(-463,106,55),(-382,146,-136),(-80,146,-138),(134,178,-82),(241,74,-129),(429,57,-84)]
    sections: list[list[kit.Vec3]] = []
    for r,s,b,k in zip(roof,shoulders,lower,keel):
        sections.append([(k[0],-k[1],k[2]),(k[0],k[1],k[2]),
                         (b[0],b[1],b[2]),(s[0],s[1],s[2]),(r[0],r[1],r[2]),
                         (r[0],-r[1],r[2]),(s[0],-s[1],s[2]),(b[0],-b[1],b[2])])
    parts = [kit.bevel(kit.loft("armoured_freighter",sections,hull),1,2,angle_deg=10)]
    def wall(x: float, z: float) -> float:
        if x < -91:
            t = (x+402)/311
            shoulder_y, shoulder_z = 118+60*t,59+19*t
        else:
            shoulder_y, shoulder_z = 178-(x+91)*14/223,78-(x+91)*11/223
        lower_z = -75-(x+381)*7/515
        t = (shoulder_z-z)/(shoulder_z-lower_z)
        return shoulder_y+(178-shoulder_y)*t
    for side in (-1,1):
        for x in (-305,-195,-85,25,100):
            outline = [(end,side*(wall(end,z)-1),z)
                       for end,z in ((x-28,-53),(x+28,-53),(x+28,7),(x-28,7))]
            parts.append(kit.loft("cargo_access_hatch",[outline,[(px,py+side*2,pz) for px,py,pz in outline]],panel))
            seam = [(x+dx,side*(wall(x+dx,z)+1),z) for dx,z in ((-1,-51),(1,-51),(1,5),(-1,5))]
            parts.append(kit.loft("hatch_seam",[seam,[(px,py+side,pz) for px,py,pz in seam]],dark))
        for x in (-330,-280,-230,-180,-130,-80,-30,20,70,120):
            outline = [(end,side*(wall(end,z)-1),z)
                       for end,z in ((x-21,-67),(x+21,-67),(x+21,-63),(x-21,-63))]
            parts.append(kit.loft("lower_hull_rail",[outline,[(px,py+side*2,pz) for px,py,pz in outline]],trim))
        parts.append(kit.loft("prow_side_panel",[
            [(x,side*(w-2),-53),(x,side*(w+2),-53),
             (x,side*(w+2),-8),(x,side*(w-2),-8)]
            for x,w in ((308,95),(372,81))],panel))
        parts.append(kit.cylinder("prow_docking_port",(325,side*91,-15),12,6,(0,side,0),trim))
        parts.append(kit.cylinder("prow_port_recess",(325,side*94,-15),8,2,(0,side,0),dark))
    # The command slit lies in the sloping prow face; the bronze edges remain subdued.
    parts.append(kit.loft("command_slit",[
        [(459,-70,-8),(459,70,-8),(448,70,-34),(448,-70,-34)],
        [(462,-70,-8),(462,70,-8),(451,70,-34),(451,-70,-34)]],dark))
    for x,z in ((300,31),(335,24),(370,17)):
        parts.append(kit.loft("prow_roof_plate",[
            [(end,-40,height-1),(end,40,height-1),(end,40,height+2),(end,-40,height+2)]
            for end,height in ((x-13,z+2.5),(x+13,z-2.5))],panel))
    parts.append(kit.sphere("command_sensor",(268,0,38),26,panel,scale=(1,.7,.2)))
    for start, end in (((-90,110,-128),(-129,110,-211)),((-116,110,-128),(-184,110,-233))):
        centre = tuple((a+b)/2 for a,b in zip(start,end))
        axis = tuple(a-b for a,b in zip(start,end))
        depth = sum(v*v for v in axis)**.5
        parts.append(kit.cylinder("ventral_antenna",centre,1.5,depth,axis,trim,segments=10))
    for y in (-72,0,72):
        parts.append(kit.cylinder("drive_socket",(-414,y,-35),23,54,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_rim",(-443,y,-35),23,8,(-1,0,0),trim,cap_end=False))
        parts.append(kit.cylinder("drive_glow",(-445,y,-35),18,2,(-1,0,0),exhaust))
    return kit.finalize(parts,length)
