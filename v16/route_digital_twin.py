from dataclasses import dataclass

@dataclass(frozen=True)
class Cell:
    network: int
    position_m: float
    radius_m: float
    quality_peak: float

class RouteTwin:
    def __init__(self,cells): self.cells=sorted(cells,key=lambda c:c.position_m)
    def predict(self, position_m, speed_mps, horizon_s=10):
        future_pos=position_m+max(0.,speed_mps)*horizon_s
        candidates=[]
        for c in self.cells:
            d=abs(c.position_m-future_pos)
            if d<=c.radius_m:
                q=max(0.,c.quality_peak*(1.-d/c.radius_m))
                candidates.append((q,c.network,future_pos))
        return max(candidates,default=(0,None,future_pos))
