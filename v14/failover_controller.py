from dataclasses import dataclass

@dataclass
class Link:
    name: str
    healthy: bool=True
    quality: float=1.0
    loss_pct: float=0.0
    latency_ms: float=20.0

class FailoverController:
    def __init__(self, links, min_quality=.05, max_loss=8.0, max_latency=200.0):
        self.links=links; self.min_quality=min_quality; self.max_loss=max_loss; self.max_latency=max_latency
        self.active=None; self.switches=0

    def eligible(self):
        return [x for x in self.links if x.healthy and x.quality>=self.min_quality and x.loss_pct<=self.max_loss and x.latency_ms<=self.max_latency]

    def select(self):
        candidates=self.eligible()
        if not candidates:
            self.active=None; return None
        best=max(candidates,key=lambda x:(x.quality, -x.loss_pct, -x.latency_ms))
        if self.active is not None and best.name!=self.active.name: self.switches+=1
        self.active=best; return best
