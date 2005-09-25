/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include "dumbfilereplayer"
#include <kio/netaccess.h>
#include <kmdcodec.h>
#include <sys/time.h>
#include <sched.h>
#include <unistd.h>

using namespace xercesc_2_7;

inline int
compareTimes(const struct timeval& t1, const struct timeval& t2)
{
  //Compare timestamps on messages
  time_t sec1=t1.tv_sec, sec2=t2.tv_sec;
  suseconds_t usec1=t1.tv_usec, usec2=t2.tv_usec;
  int res=(sec1==sec2)?((usec1==usec2)?0:((usec1>usec2)?1:-1))
                      :((sec1>sec2)?1:-1);      
  return res;
}

inline struct timeval
operator-(struct timeval t1, struct timeval t2)
{
  struct timeval res;
  res.tv_sec=t1.tv_sec-t2.tv_sec;
  res.tv_usec=t1.tv_usec-t2.tv_usec;
  if(res.tv_sec>0)
  {
    if(res.tv_usec>999999) { res.tv_sec+=1; res.tv_usec-=1000000; }
    else if(res.tv_usec<0) { res.tv_sec-=1; res.tv_usec+=1000000; }
  }
  else if(res.tv_sec<0)
  {
    if(res.tv_usec<-999999) { res.tv_sec-=1; res.tv_usec+=1000000; }
    else if(res.tv_usec>0) { res.tv_sec+=1; res.tv_usec-=1000000; }
  }
  else
  {
    if(res.tv_usec>999999) { res.tv_sec+=1; res.tv_usec-=1000000; }
    else if(res.tv_usec<-999999) { res.tv_sec-=1; res.tv_usec+=1000000; }
  }
                  
  return res;
}

inline struct timeval
esleep(struct timeval t)
{
  struct timeval res;
  struct timezone czone;    
  
  if(t.tv_sec==0 && t.tv_usec>0) usleep(t.tv_usec);
  else if(t.tv_sec>0) 
  {
    res.tv_sec=t.tv_sec;
    while(0!=(res.tv_sec=sleep(res.tv_sec))){};    
  }
  if(-1==gettimeofday(&res,&czone))
  {
    res.tv_sec=0; res.tv_usec=0;
  }    
  return res;
}

int
DumbFileReplayer::OrdMsgList::compareItems(QPtrCollection::Item item1, 
                                           QPtrCollection::Item item2)
{  
  return compareTimes(reinterpret_cast<KludgeMsg*>(item1)->t,
                      reinterpret_cast<KludgeMsg*>(item2)->t);  
}

DumbFileReplayer::DataReplayer::DataReplayer(DumbFileReplayer* parent, 
                                             const char* name)
                               :KludgeSource(parent,name)
{  
  my_data.setAutoDelete(true);
  autorepeat=true;
  still_good=true; 
  data_time_range.first.tv_sec=LONG_MAX;
  data_time_range.first.tv_usec=999999;
  data_time_range.second.tv_sec=0; data_time_range.second.tv_usec=0;
  my_msg.origin=this;
}

DumbFileReplayer::DataReplayer::~DataReplayer()
{
  
}

void
DumbFileReplayer::DataReplayer::setParams(const QDict<QString>& params)
{
  QString *pval;
  sim_source=(0!=(pval=params[QString("data_source")]))?*pval:QString();
  sim_group=(0!=(pval=params[QString("data_group")]))?*pval:QString();
  if(0!=(pval=params[QString("autorepeat")]))    
    autorepeat=(*pval==QString("true"));
}

void
DumbFileReplayer::DataReplayer::setDataMsg(KludgeMsg* n_msg)
{
  my_data.inSort(n_msg);  
  if(0>compareTimes(n_msg->t,data_time_range.first))
    data_time_range.first=n_msg->t;
  if(0<compareTimes(n_msg->t,data_time_range.second))
    data_time_range.second=n_msg->t;    
}

bool
DumbFileReplayer::DataReplayer::update()
{
  struct timeval ctime;
  struct timezone czone;
  KludgeMsg *t_msg;  

  if(-1==gettimeofday(&ctime,&czone)) return false;
  if(0==(t_msg=my_data.getFirst())) return false;  
  if(my_data.current()==t_msg)
  {   
    base_delta=ctime;    
    my_msg.t=ctime; my_msg.v=t_msg->v;
    send(my_msg);
    my_data.next();
    return true;
  }
  else if(my_data.current()==0)
  {    
    my_data.first();
    return autorepeat;
  }
  else 
  {       
    my_msg.t=esleep((my_data.current()->t - data_time_range.first) -
                    (ctime - base_delta));
    my_msg.v=my_data.current()->v;
    send(my_msg);
    my_data.next();       
    return true;
  }        
}

QString& 
DumbFileReplayer::DataReplayer::simGroup()
{
  return sim_group;
}

QString& 
DumbFileReplayer::DataReplayer::simSource()
{
  return sim_source;
}

void
DumbFileReplayer::DataReplayer::resetData()
{
  my_data.clear();
  still_good=true;
}

KludgeSource*
DumbFileReplayer::newSource(const char* src_name)
{
  return new DataReplayer(this,src_name);
}

QByteArray DumbFileReplayer::DumbFileLoader::schema_exp;
#include "dumbfileloader.res"

MemBufInputSource*
DumbFileReplayer::DumbFileLoader::getEmbeddedSchema()
{
  if(schema_exp.isEmpty())
    schema_exp=qUncompress(z_schema,sizeof(z_schema));  

  return new MemBufInputSource((const unsigned char*)schema_exp.data(),
                               schema_exp.size(),"kludgedata_schema",false);
}

bool
DumbFileReplayer::loadXMLData()
{
  if(!data_url.hasPath()) return false;
  bool res;
  logMsg(QString("Starting data loading: ") + data_url.prettyURL(),0,this);

  QString tmpfile;
  if(!KIO::NetAccess::download(data_url,tmpfile,0))
  {
    logMsg(QString("Loading of ") + data_url.prettyURL() +
           QString(" failed!"),50,this);
    return false;
  }

  res=parseXMLData(tmpfile);
  
  KIO::NetAccess::removeTempFile(tmpfile);
  if(!res) logMsg(QString("XML parsing failed. Check your data file!"),50,this);
    
  return res;    
}

bool
DumbFileReplayer::parseXMLData(const QString& file_name)
{
  bool res=true;
  XMLPlatformUtils::Initialize();
  LocalFileInputSource *my_data;
  try
  {
    my_data = new LocalFileInputSource(file_name.ucs2());
  }
    catch(const XMLException& toCatch)
    {
      logMsg(QString("SAX parsing error - ") +
             QString::fromUcs2(toCatch.getMessage()),50,this);
      XMLPlatformUtils::Terminate();
      return false;
    }
  SAX2XMLReader *parser = XMLReaderFactory::createXMLReader();
  parser->setProperty(XMLUni::fgXercesScannerName,(void*)XMLUni::fgSGXMLScanner);
  parser->setFeature(XMLUni::fgSAX2CoreValidation,true);
  parser->setFeature(XMLUni::fgSAX2CoreNameSpaces,true);
  parser->setFeature(XMLUni::fgXercesSchemaFullChecking,true);
  parser->setFeature(XMLUni::fgXercesValidationErrorAsFatal,true);
  parser->setFeature(XMLUni::fgXercesDynamic,true);
  parser->setFeature(XMLUni::fgXercesSchema,true);

  DumbFileLoader *data_handler = new DumbFileLoader(this);
  MemBufInputSource *my_schema = data_handler->getEmbeddedSchema();
  parser->setContentHandler(data_handler);
  parser->setErrorHandler(data_handler);
  parser->loadGrammar(*my_schema,Grammar::SchemaGrammarType,true);
    
  try
  {
    parser->parse(*my_data);
  }
    catch(const XMLException& toCatch)
    {
      logMsg(QString("SAX parsing error - ") +
             QString::fromUcs2(toCatch.getMessage()),50,this);
      res=false;
    }
    catch(const SAXParseException& toCatch)
    {
      logMsg(QString("SAX parsing error - ") +
             QString::fromUcs2(toCatch.getMessage()),50,this);
      res=false;
    }
    catch(...)
    {
      logMsg(QString("Evil deed just happened in XML parser!"),100,this);
      res=false;
    }


  delete parser;  
  delete data_handler;
  delete my_schema;
  delete my_data;
  XMLPlatformUtils::Terminate();
  return res;  
}

DumbFileReplayer::ReplayerMap::ReplayerMap(DumbFileReplayer* parent)
{
  mp=parent;
  ng_ns_handler=0;
  g_s_handler.setAutoDelete(true);
  mp->source_hive_lock.lock();
  QDictIterator<KludgeSource> cs(mp->source_hive);
  DataReplayer *tdr;
  
  for(;cs.current();++cs)
  {
    tdr=dynamic_cast<DataReplayer*>(cs.current());
    if(tdr->simGroup().isEmpty())
    {
      if(tdr->simSource().isEmpty()) ng_ns_handler=tdr;
      else ng_s_handler.replace(tdr->simSource(),tdr);
    }
    else
    {
      if(tdr->simSource().isEmpty()) g_ns_handler.replace(tdr->simGroup(),tdr);
      else
      {
        if(0==g_s_handler[tdr->simGroup()])
          g_s_handler.insert(tdr->simGroup(),new QDict<DataReplayer>);
        g_s_handler[tdr->simGroup()]->replace(tdr->simSource(),tdr);
      }
    }    
  }  
}

DumbFileReplayer::ReplayerMap::~ReplayerMap()
{
  mp->source_hive_lock.unlock();  
}

DumbFileReplayer::DataReplayer*
DumbFileReplayer::ReplayerMap::getReplayer(const QString* sim_group, 
                                           const QString* sim_source)
{
  if(sim_group==0)
  {
    if(sim_source==0) return ng_ns_handler;
    else return ng_s_handler[*sim_source];
  }
  else
  {
    if(sim_source==0) return g_ns_handler[*sim_group];
    else
    {
      QDict<DataReplayer> *t_dict=g_s_handler[*sim_group];
      if(t_dict!=0) return (*t_dict)[*sim_source];
      else return 0; 
    }
  }
}

DumbFileReplayer::DumbFileLoader::DumbFileLoader(DumbFileReplayer* parent)
{
  data_sources=new ReplayerMap(parent);
  parse_startelement=0;
  parse_endelement=0;
  data_count=0;
  tmp_data_fill=0;
}

DumbFileReplayer::DumbFileLoader::~DumbFileLoader()
{
  delete data_sources;
}

DumbFileReplayer::DumbFileReplayer(QObject* parent, const char* name)
                 :KludgeSrcGroup(parent,name)
{
}

DumbFileReplayer::~DumbFileReplayer()
{
  passivate();
}

void
DumbFileReplayer::setParams(const QDict<QString>& params)
{
  data_url=KURL(*params[QString("url")]);
}

void
DumbFileReplayer::activate()
{
  if(!running() && loadXMLData())
  {
    go_on=true;
    start();
  }
}

void
DumbFileReplayer::passivate()
{
  go_on=false;  
  if(running()) wait();  
  source_hive_lock.lock();
  QDictIterator<KludgeSource> cs(source_hive);
  DataReplayer *tdr;
  
  for(;cs.current();++cs)
  {
    tdr=dynamic_cast<DataReplayer*>(cs.current());
    tdr->resetData();    
  }
  source_hive_lock.unlock();
}

void
DumbFileReplayer::run()
{
  DataReplayer *cur_src;
  source_hive_lock.lock();
  QDictIterator<KludgeSource> src_list(source_hive);
  source_hive_lock.unlock();
  bool any_good;
  while(go_on)
  {    
    source_hive_lock.lock();   
    any_good=false;
    src_list.toFirst();
    while(0!=(cur_src=dynamic_cast<DataReplayer*>(src_list())))
    {      
      if(cur_src->still_good)
         cur_src->still_good=cur_src->update();

      if(cur_src->still_good) any_good=true;
      else 
      {
        logMsg(QString("Source ") + QString(cur_src->name()) +
               QString(" terminated"),0,this);
        source_hive.remove(QString::fromUtf8(cur_src->name()));
      }
    }
    source_hive_lock.unlock();
    if(!any_good) break;
    sched_yield();
  }
}

void
DumbFileReplayer::DumbFileLoader::error(const SAXParseException& exception)
{
  
}

void
DumbFileReplayer::DumbFileLoader::fatalError(const SAXParseException& exception)
{

}

void
DumbFileReplayer::DumbFileLoader::convertAttributes(QDict<QString>& q_attr,
                                                    const Attributes& attrs)
{
  for(unsigned cnt=0;cnt<attrs.getLength();cnt++)
    q_attr.insert(QString::fromUcs2(attrs.getLocalName(cnt)),
                  new QString(QString::fromUcs2(attrs.getValue(cnt))));
}

void
DumbFileReplayer::DumbFileLoader::startElement(const XMLCh* const uri,
                                               const XMLCh* const localname,
                                               const XMLCh* const qname,
                                               const Attributes& attrs)
{
  if(parse_startelement)
    (this->*parse_startelement)(uri,localname,qname,attrs);
  else
  {
    QString tag_name=QString::fromUcs2(localname);
    if(tag_name==QString("kludgeData"))
    {      
      parse_startelement=&DumbFileReplayer::DumbFileLoader::kludgeDataStart;
      parse_endelement=&DumbFileReplayer::DumbFileLoader::kludgeDataEnd;
    }
  }
}

void
DumbFileReplayer::DumbFileLoader::endElement(const XMLCh* const uri,
                                             const XMLCh* const localname,
                                             const XMLCh* const qname)
{
  if(parse_endelement)
    (this->*parse_endelement)(uri,localname,qname);
}

void
DumbFileReplayer::DumbFileLoader
                ::kludgeDataStart(const XMLCh* const uri,
                                  const XMLCh* const localname,
                                  const XMLCh* const qname,
                                  const xercesc_2_7::Attributes& attrs)
{
  QString tag_name=QString::fromUcs2(localname);
  if(tag_name==QString("datums"))
  {
    parse_startelement=&DumbFileReplayer::DumbFileLoader::datumsStart;
    parse_endelement=&DumbFileReplayer::DumbFileLoader::datumsEnd;
  }  
}

void
DumbFileReplayer::DumbFileLoader::kludgeDataEnd(const XMLCh* const uri,
                                                const XMLCh* const localname,
                                                const XMLCh* const qname)
{  
}

void
DumbFileReplayer::DumbFileLoader
                ::datumsStart(const XMLCh* const uri,
                              const XMLCh* const localname,
                              const XMLCh* const qname,
                              const xercesc_2_7::Attributes& attrs)
{
  QString tag_name=QString::fromUcs2(localname);
  QDict<QString> q_attr;
  q_attr.setAutoDelete(true);
  convertAttributes(q_attr,attrs);

  if(tag_name==QString("d"))
  {
    cur_src=data_sources->getReplayer(q_attr[QString("group")],
                                      q_attr[QString("source")]);
    if(cur_src!=0)
    {
      data_count=q_attr[QString("length")]->toUInt();
      tmp_data.resize((sizeof(double)*data_count+sizeof(struct timeval))*4/3+8); 
      tmp_data_fill=0;
    }
    else
    {
      tmp_data.resize(0);
      tmp_data_fill=0;
    }
  }
}

void
DumbFileReplayer::DumbFileLoader::datumsEnd(const XMLCh* const uri,
                                            const XMLCh* const localname,
                                            const XMLCh* const qname)
{
  QString tag_name=QString::fromUcs2(localname); 
  if(tag_name==QString("d"))
  {
    if(cur_src!=0)
    {      
      QByteArray raw_data;      
      KCodecs::base64Decode(tmp_data,raw_data);      
      KludgeMsg *t_msg=new KludgeMsg();
      t_msg->origin=0;
      t_msg->t=*(reinterpret_cast<struct timeval*>(raw_data.data()));
      t_msg->v.resize(data_count);
      memcpy(reinterpret_cast<char*>(t_msg->v.data()),
             raw_data.data()+sizeof(struct timeval),data_count*sizeof(double));
      cur_src->setDataMsg(t_msg);
      cur_src=0;
    }    
  }
  else if(tag_name==QString("datums"))
  {
    parse_startelement=&DumbFileReplayer::DumbFileLoader::kludgeDataStart;
    parse_endelement=&DumbFileReplayer::DumbFileLoader::kludgeDataEnd;
  }
}

void
DumbFileReplayer::DumbFileLoader::characters(const XMLCh* const chars,
                                             const unsigned int length)
{  
  QByteArray xchars(length);
  for(int cnt=0;cnt<length;cnt++)  
    xchars[cnt]=*(reinterpret_cast<const char*>(chars)+2*cnt);    
     
  unsigned copy_size=(tmp_data.size()-tmp_data_fill)<?length;  
  if(copy_size==0) return;
  memcpy(tmp_data.data()+tmp_data_fill,xchars.data(),copy_size);
  tmp_data_fill+=copy_size;  
}

#include "dumbfilereplayer.moc"
