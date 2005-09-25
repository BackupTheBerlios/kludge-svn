/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include "kludgesource"

namespace std_c
{
#include <math.h>
#include <sched.h>
}
#include <sys/time.h>

KludgeSource::KludgeSource(QObject* parent, const char* name)
             :QObject(parent,name)
{
  sink_names.setAutoDelete(true);  
}

KludgeSource::~KludgeSource()
{
  
}

void 
KludgeSource::regSink(const QString& gname, const QString& sname)
{
  QMutexLocker sink_names_is_safe(&sink_names_lock);
  QStringList *snl=sink_names[gname];
  
  if(!snl)      
  {
    snl=new QStringList;
    sink_names.insert(gname,snl);    
  }    
  snl->append(sname);
}

QStringList* 
KludgeSource::getPending(const QString& gname)
{
  QMutexLocker sink_names_is_safe(&sink_names_lock);
  return sink_names[gname];
}

double
KludgeFGen::WaveGen::sine(struct timeval t)
{
  double td=((double)t.tv_usec)*0.000001;
  td+=(double)(t.tv_sec & 0x7fffffff);
  double om=2*M_PI*freq*td+phase;
  om=std_c::fmod(om,2*M_PI);
  return std_c::sin(om);
}

KludgeFGen::WaveGen::WaveGen(QObject* parent, const char* name)
                    :KludgeSource(parent,name)
{
  last_time.tv_sec=0; last_time.tv_usec=0;
  eval=&WaveGen::sine;
  freq=1.0; phase=0.0;
  my_msg.v.resize(1);
  my_msg.origin=this; 
}

KludgeFGen::WaveGen::~WaveGen()
{
  
}

void
KludgeFGen::WaveGen::setParams(const QDict<QString>& params)
{
  QString *val; bool is_good; double tval;
  if(0!=(val=params["freq"]))
  {
    tval=val->toDouble(&is_good);
    freq=(is_good)?tval:1.0;
  }
  if(0!=(val=params["phase"]))
  {
    tval=val->toDouble(&is_good);
    phase=(is_good)?tval:1.0;
  }
  if(0!=(val=params["type"]))
  {
    if(*val==QString("sine")) eval=&WaveGen::sine;
  }
}

void
KludgeFGen::WaveGen::update()
{
  struct timeval ctime;
  struct timezone czone;
  
  if(-1==gettimeofday(&last_time,&czone)) last_time.tv_usec+=1000;
  my_msg.v[0]=(this->*eval)(last_time);
  my_msg.t=last_time;  
  send(my_msg);    
}

KludgeSrcGroup::KludgeSrcGroup(QObject* parent, const char* name)
               :QObject(parent,name)
{
  source_hive.setAutoDelete(true);
}
 
KludgeFGen::KludgeFGen(QObject* parent, const char* name)
           :KludgeSrcGroup(parent,name)
{
  go_on=false;
}                       

KludgeFGen::~KludgeFGen()
{  
  passivate();
}

KludgeSource*
KludgeFGen::newSource(const char* src_name)
{
  return new WaveGen(this,src_name);
}

KludgeSource* 
KludgeSrcGroup::getSource(const QString& name, const QDict<QString>* params)
{
  QMutexLocker source_hive_is_safe(&source_hive_lock);
  
  KludgeSource *res=source_hive[name];
  if(res!=0 && params!=0)
  {
    res->setParams(*params);
  }
  else if(res==0 && params!=0)
  {
    res=newSource(name.utf8());
    res->setParams(*params);
    source_hive.insert(name,res);
  }
  return res;  
}

void 
KludgeSrcGroup::makeConnections(KludgeSinkGroup& sgrp)
{  
  QMutexLocker source_hive_is_safe(&source_hive_lock);
  QString sg_name(QString::fromUtf8(sgrp.name()));
  QDictIterator<KludgeSource> cs(source_hive);
  QStringList *s_list; QStringList::iterator s_list_it;
  KludgeSink *t_sink;
  
  for(;cs.current();++cs)
  {
    s_list=cs.current()->getPending(sg_name);    
    if(s_list && !s_list->isEmpty())
    {
      s_list_it=s_list->begin();
      while(s_list_it!=s_list->end())
      {
        t_sink=sgrp.getSink(*s_list_it);
        if(t_sink)
        {
          connect(cs.current(),SIGNAL(send(const KludgeMsg&)),
                  t_sink,SLOT(collector(const KludgeMsg&)));
          s_list_it=s_list->remove(s_list_it);
        }
        else
          ++s_list_it;
      }
    }  
  }    
}

void
KludgeFGen::run()
{
  WaveGen *cur_src;
  source_hive_lock.lock();
  QDictIterator<KludgeSource> src_list(source_hive);        
  source_hive_lock.unlock();  
  while(go_on)
  {
    source_hive_lock.lock();
    src_list.toFirst();
    while(0!=(cur_src=dynamic_cast<WaveGen*>(src_list())))
    {
      cur_src->update();
    }
    source_hive_lock.unlock();
    std_c::sched_yield();
  }
}   
     
void 
KludgeFGen::activate()
{
  if(!running())
  {    
    go_on=true;
    start();
  }
}

void 
KludgeFGen::passivate()
{
  go_on=false;
  if(running()) wait();
}
                    
#include "kludgesource.moc"
