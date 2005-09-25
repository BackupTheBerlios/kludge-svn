/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include "dumbfilewriter"
#include <kio/netaccess.h>
#include <kmdcodec.h>

const char DumbFileWriter::FileLogger::datafile_header[]=
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"                             \
  "<kludgeData xmlns=\"kludgedata.v1.it.is\"\n"                              \
  "            xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"    \
  "            xsi:schemaLocation=\"kludgedata.v1.it.is kludgedata.xsd\">\n" \
  " <datums>\n";
const char DumbFileWriter::FileLogger::datafile_footer[]=  
  " </datums>\n"        \
  "</kludgeData>\n";

DumbFileWriter::FileLogger::FileLogger(DumbFileWriter* parent, 
                                       const char* name)
                           :KludgeSink(parent,name)
{
  tmp_log.setAutoDelete(true);        
  fprintf(tmp_log.fstream(),datafile_header);
}

void 
DumbFileWriter::FileLogger::collector(const KludgeMsg& msg)
{  
  QString opt1, opt2;
  
  if(msg.origin)
  {
    if(msg.origin->parent() && msg.origin->parent()->name())
      opt1=QString(" group=\"") + 
           QString::fromUtf8(msg.origin->parent()->name()) +
           QString("\" ");
    if(msg.origin->name())
      opt2=QString(" source=\"") +
           QString::fromUtf8(msg.origin->name()) +
           QString("\" ");
  }
  QByteArray raw_data((msg.v.count()+1)*sizeof(double));
  QByteArray b64_data;
  
  memcpy(raw_data.data(),reinterpret_cast<const char*>(&msg.t),
         sizeof(struct timeval));
  memcpy(raw_data.data()+sizeof(struct timeval),
         reinterpret_cast<const char*>(msg.v.data()),
         sizeof(double)*msg.v.count());
  KCodecs::base64Encode(raw_data,b64_data,true);
  tmp_log_serializer.lock();  
  fprintf(tmp_log.fstream(),"  <d%s%s length=\"%d\">\n",
          (const char*)(opt1.utf8()),(const char*)(opt2.utf8()),msg.v.count());
  fwrite(b64_data.data(),1,b64_data.count(),tmp_log.fstream());
  fprintf(tmp_log.fstream(),"\n  </d>\n");
  tmp_log_serializer.unlock();
}

DumbFileWriter::FileLogger::~FileLogger()
{  
  fprintf(tmp_log.fstream(),datafile_footer);
  if(tmp_log.close() && target_url.hasPath())   
    KIO::NetAccess::upload(tmp_log.name(),target_url,0);
}

void
DumbFileWriter::FileLogger::setParams(const QDict<QString>& params)
{
  QString *pval;
  pval=params[QString("url")];
  if(pval) target_url=KURL(*pval);
}

DumbFileWriter::DumbFileWriter(QObject* parent, const char* name)
               :KludgeSinkGroup(parent,name)
{
}               

DumbFileWriter::~DumbFileWriter()
{
}

KludgeSink*
DumbFileWriter::getSink(const QString& name, const QDict<QString>* params)
{
  QMutexLocker sink_hive_is_safe(&sink_hive_lock);
  KludgeSink *res=sink_hive[name];
  if(res!=0)
  {
    if(params!=0) res->setParams(*params);
  }
  else if(res==0 && params!=0)
  {
    res=new FileLogger(this,name.utf8());
    if(params!=0) res->setParams(*params);
    sink_hive.insert(name,res);    
  }
  return res;
}

#include "dumbfilewriter.moc"
