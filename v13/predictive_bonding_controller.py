from __future__ import annotations
import pandas as pd
from sklearn.ensemble import RandomForestClassifier

FEATURES=['train_position','n1_quality','n2_quality','n3_quality','n1_latency','n2_latency','n3_latency','n1_loss','n2_loss','n3_loss','n1_load','n2_load','n3_load']
LINKS=(1,2,3)

def train(dataset='railway-v8.2-dataset.csv'):
    df=pd.read_csv(dataset).sort_values(['seed','time'])
    df['future_link']=df.groupby('seed')['selected_link'].shift(-1)
    df=df.dropna(subset=['future_link']).copy(); df['future_link']=df['future_link'].astype(int)
    tr=df[df.seed<1049]; te=df[df.seed>=1049]
    model=RandomForestClassifier(n_estimators=400,random_state=42,class_weight='balanced',min_samples_leaf=2,n_jobs=-1)
    model.fit(tr[FEATURES],tr.future_link)
    te=te.copy(); te['predicted_link']=model.predict(te[FEATURES])
    return model,te

def choose_bonded(row, predicted_link):
    q={1:row.n1_quality,2:row.n2_quality,3:row.n3_quality}
    loss={1:row.n1_loss,2:row.n2_loss,3:row.n3_loss}
    cap={1:30.,2:20.,3:15.}
    usable={i:cap[i]*max(0.,min(1.,q[i]))*(1.-max(0.,min(100.,loss[i]))/100.) for i in LINKS if q[i]>.05}
    if not usable: return []
    preferred=predicted_link if predicted_link in usable else max(usable,key=usable.get)
    return [preferred]+[i for i in sorted(usable,key=usable.get,reverse=True) if i!=preferred]

if __name__=='__main__':
    model, test=train()
    acc=(test.predicted_link==test.future_link).mean()
    print(f'V13 predictive-bonding model; unseen-seed accuracy={acc*100:.2f}%')
