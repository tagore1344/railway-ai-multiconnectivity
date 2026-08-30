from dataclasses import dataclass

@dataclass
class PassengerClass:
    name: str
    passengers: int
    min_mbps_per_passenger: float
    priority: int


def allocate(total_capacity_mbps, classes):
    demand={c.name:c.passengers*c.min_mbps_per_passenger for c in classes}
    allocation={k:0.0 for k in demand}
    remaining=total_capacity_mbps
    for c in sorted(classes,key=lambda x:x.priority,reverse=True):
        grant=min(demand[c.name],remaining)
        allocation[c.name]=grant; remaining-=grant
    if remaining>0 and sum(demand.values())>0:
        for c in classes:
            allocation[c.name]+=remaining*demand[c.name]/sum(demand.values())
    return allocation

if __name__=='__main__':
    classes=[PassengerClass('emergency',1,2.0,100),PassengerClass('interactive',6,.8,60),PassengerClass('bulk',12,.2,20)]
    print(allocate(30,classes))
