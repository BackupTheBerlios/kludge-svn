/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include "kludgemainwidget"
#include "kludgecfgreader"
#include "expertdaq"
#include "dumbfilewriter"
#include "dumbfilereplayer"

#include <kurl.h>
#include <kio/netaccess.h> 
#include <kmdcodec.h>

using namespace xercesc_2_7;

QByteArray KludgeCfgReader::schema_exp;
#include "kludgecfgreader.res"

void 
KludgeCfgReader::convertAttributes(QDict<QString>& q_attr,
                                   const Attributes& attrs)
{
  for(unsigned cnt=0;cnt<attrs.getLength();cnt++)
    q_attr.insert(QString::fromUcs2(attrs.getLocalName(cnt)),
                  new QString(QString::fromUcs2(attrs.getValue(cnt))));
}
                         
KludgeCfgReader::KludgeCfgReader(KludgeHubProvider& hub_srv,
                                 QWidget* parent, const char* name)
                    :QObject(parent,name)
{    
  stored_hub_srv=&hub_srv;
  my_hub_srv=0;
  tmp_src_grp=0;
  tmp_sink_grp=0;
  tmp_img=0;
  parse_startelement=0;
  parse_endelement=0;
  is_good=true;
}

KludgeCfgReader::~KludgeCfgReader()
{
  if(tmp_src_grp) delete tmp_src_grp;
  if(tmp_sink_grp) delete tmp_sink_grp;
  if(my_hub_srv) my_hub_srv->hubReset();
}

void 
KludgeCfgReader::kludgeAppStart(const XMLCh* const uri, 
                                const XMLCh* const localname,
                                const XMLCh* const qname, 
                                const xercesc_2_7::Attributes& attrs)
{
  QString tag_name=QString::fromUcs2(localname);
  if(tag_name==QString("kludgeSources"))
  {
    parse_startelement=&KludgeCfgReader::kludgeSourcesStart;
    parse_endelement=&KludgeCfgReader::kludgeSourcesEnd;
  }
  else if(tag_name==QString("kludgeSinks"))
  {
    parse_startelement=&KludgeCfgReader::kludgeSinksStart;
    parse_endelement=&KludgeCfgReader::kludgeSinksEnd;
  }
  else if(tag_name==QString("kludgeGUI"))
  {
    parse_startelement=&KludgeCfgReader::kludgeGUIStart;
    parse_endelement=&KludgeCfgReader::kludgeGUIEnd;
  }
}
    
void 
KludgeCfgReader::kludgeAppEnd(const XMLCh* const uri, 
                              const XMLCh* const localname, 
                              const XMLCh* const qname)
{
  my_hub_srv=0;
}

void 
KludgeCfgReader::kludgeSourcesStart(const XMLCh* const uri, 
                                    const XMLCh* const localname,
                                    const XMLCh* const qname, 
                                    const xercesc_2_7::Attributes& attrs)
{  
  QString tag_name=QString::fromUcs2(localname);
  QDict<QString> q_attr; 
  q_attr.setAutoDelete(true);
  convertAttributes(q_attr,attrs);
  
  if(tag_name==QString("kludgeFGen"))
  {
    tmp_src_grp=new KludgeFGen(0,q_attr[QString("name")]->utf8());
    tmp_src_grp->setParams(q_attr);
    logMsg(QString("New KludgeFGen element: ") + *q_attr[QString("name")],
           0,this);     
  }
  else if(tag_name==QString("expertDaq"))
  {
    tmp_src_grp=new ExpertDaq(0,q_attr[QString("name")]->utf8());
    tmp_src_grp->setParams(q_attr);
    logMsg(QString("New ExpertDaq element: ") + *q_attr[QString("name")],
           0,this);     
  }
  else if(tag_name==QString("dumbFileReplayer"))
  {
    tmp_src_grp=new DumbFileReplayer(0,q_attr[QString("name")]->utf8());
    tmp_src_grp->setParams(q_attr);
    logMsg(QString("New DumbFileReplayer element: ") + *q_attr[QString("name")],
           0,this);     
  }
  else if(tag_name==QString("source"))
  {
    tmp_src=tmp_src_grp->getSource(*q_attr[QString("name")],&q_attr);
  }
  else if(tag_name==QString("sink"))
  {
    tmp_src->regSink(*q_attr[QString("group")],
                     *q_attr[QString("name")]);   
  }  
}
    
void 
KludgeCfgReader::kludgeSourcesEnd(const XMLCh* const uri, 
                                  const XMLCh* const localname, 
                                  const XMLCh* const qname)
{
  QString tag_name=QString::fromUcs2(localname);
  if(tag_name==QString("kludgeSources"))
  {
    parse_startelement=&KludgeCfgReader::kludgeAppStart;
    parse_endelement=&KludgeCfgReader::kludgeAppEnd;
  }
  else if(tag_name==QString("kludgeFGen"))
  {
    my_hub_srv->regSrcGroup(tmp_src_grp);
    tmp_src_grp=0;    
  }
  else if(tag_name==QString("expertDaq"))
  {
    my_hub_srv->regSrcGroup(tmp_src_grp);
    tmp_src_grp=0;    
  }
  else if(tag_name==QString("dumbFileReplayer"))
  {
    my_hub_srv->regSrcGroup(tmp_src_grp);
    tmp_src_grp=0;    
  }
  else if(tag_name==QString("source"))
  {
    tmp_src=0;
  }
  else if(tag_name==QString("sink"))
  {
  }
}

void 
KludgeCfgReader::kludgeSinksStart(const XMLCh* const uri, 
                                  const XMLCh* const localname,
                                  const XMLCh* const qname, 
                                  const Attributes& attrs)
{
  QString tag_name=QString::fromUcs2(localname);
  QDict<QString> q_attr; 
  q_attr.setAutoDelete(true);
  convertAttributes(q_attr,attrs);
  
  if(tag_name==QString("kludgeIMap"))
  {   
    tmp_sink_grp=new KludgeIMap(0,q_attr[QString("name")]->utf8());    
    tmp_sink_grp->setParams(q_attr);
    logMsg(QString("New KludgeIMap element: ") + *q_attr[QString("name")],
           0,this); 
  }
  else if(tag_name==QString("dumbFileWriter"))
  {
    tmp_sink_grp=new DumbFileWriter(0,q_attr[QString("name")]->utf8());    
    tmp_sink_grp->setParams(q_attr);
    logMsg(QString("New DumbFileWriter element: ") + *q_attr[QString("name")],
           0,this); 
  }
  else if(tag_name==QString("sink"))
  {    
    tmp_sink=tmp_sink_grp->getSink(*q_attr[QString("name")],&q_attr);
  }
  else if(tag_name==QString("source"))
  {   
    tmp_sink->regSrc(*q_attr[QString("group")],
                     *q_attr[QString("name")]);   
  }
  else if(tag_name==QString("image"))
  {        
    tmp_img=0;
    if(q_attr[QString("url")]) tmp_img=loadQImage(*q_attr[QString("url")]); 
    if(tmp_img==0) tmp_data.resize(0); //try to get embedded image    
  }
}

void 
KludgeCfgReader::kludgeSinksEnd(const XMLCh* const uri, 
                                const XMLCh* const localname, 
                                const XMLCh* const qname)
{
  QString tag_name=QString::fromUcs2(localname);
  if(tag_name==QString("kludgeSinks"))
  {
    parse_startelement=&KludgeCfgReader::kludgeAppStart;
    parse_endelement=&KludgeCfgReader::kludgeAppEnd;
  }
  else if(tag_name==QString("kludgeIMap"))
  {
    if(tmp_img) dynamic_cast<KludgeIMap*>(tmp_sink_grp)->setImage(*tmp_img);
    my_hub_srv->regSinkGroup(tmp_sink_grp);
    tmp_sink_grp=0;
    tmp_img=0;
  }
  else if(tag_name==QString("dumbFileWriter"))
  {    
    my_hub_srv->regSinkGroup(tmp_sink_grp);
    tmp_sink_grp=0;
    tmp_img=0;
  }
  else if(tag_name==QString("sink"))
  {
    tmp_sink=0;
  }
  else if(tag_name==QString("source"))
  {
  }  
  else if(tag_name==QString("image"))
  {
    if(tmp_img==0 && tmp_data.size()>0)
    {
      QByteArray img_data;
      KCodecs::base64Decode(tmp_data,img_data);
      tmp_img=new QImage(img_data);
      if(tmp_img->isNull())
      {
        delete tmp_img;
        tmp_img=0;
      }        
      tmp_data.resize(0);
    }
  }
}

void 
KludgeCfgReader::kludgeGUIStart(const XMLCh* const uri, 
                                const XMLCh* const localname,
                                const XMLCh* const qname, 
                                const xercesc_2_7::Attributes& attrs)
{
  QString tag_name=QString::fromUcs2(localname);
  QDict<QString> q_attr; 
  q_attr.setAutoDelete(true);
  convertAttributes(q_attr,attrs);
  
  if(tag_name==QString("providesSinkGroup"))
  {
    tmp_sink_grp=my_hub_srv->getSinkGroup(*q_attr[QString("name")]);       
    if(tmp_sink_grp && my_hub_srv->hasGUI())
    {                  
      dynamic_cast<CairoItem*>(tmp_sink_grp)->cairoAttach(my_hub_srv->hasGUI());
    }
    tmp_sink_grp=0;    
  }  
}
    
void 
KludgeCfgReader::kludgeGUIEnd(const XMLCh* const uri, 
                              const XMLCh* const localname, 
                              const XMLCh* const qname)
{
  QString tag_name=QString::fromUcs2(localname);
  if(tag_name==QString("kludgeGUI"))
  {
    parse_startelement=&KludgeCfgReader::kludgeAppStart;
    parse_endelement=&KludgeCfgReader::kludgeAppEnd;
  }
  
}
    
void 
KludgeCfgReader::startElement(const XMLCh* const uri, 
                              const XMLCh* const localname,
                              const XMLCh* const qname,
                              const Attributes& attrs)
{  
  if(parse_startelement)
    (this->*parse_startelement)(uri,localname,qname,attrs);
  else
  {
    QString tag_name=QString::fromUcs2(localname);
    if(tag_name==QString("kludgeApp"))
    {
      my_hub_srv=stored_hub_srv;
      parse_startelement=&KludgeCfgReader::kludgeAppStart;
      parse_endelement=&KludgeCfgReader::kludgeAppEnd;
    }
  }      
}

void 
KludgeCfgReader::endElement(const XMLCh* const uri, 
                            const XMLCh* const localname, 
                            const XMLCh* const qname)
{
  if(parse_endelement)
    (this->*parse_endelement)(uri,localname,qname);  
} 

void 
KludgeCfgReader::characters(const XMLCh* const chars, 
                            const unsigned int length)
{
  QByteArray xchars(length);
  for(int cnt=0;cnt<length;cnt++)  
    xchars[cnt]=*(reinterpret_cast<const char*>(chars)+2*cnt);    
     
  unsigned long tmp_data_fill=tmp_data.size();
  tmp_data.resize(length+tmp_data_fill);  
  memcpy(tmp_data.data()+tmp_data_fill,xchars.data(),length);  
}

void 
KludgeCfgReader::error(const SAXParseException& exception)
{
  logMsg(QString("SAX parsing error - ") + 
         QString::fromUcs2(exception.getMessage()),50,this); 
  is_good=false;
}

void 
KludgeCfgReader::fatalError(const SAXParseException& exception)
{
  logMsg(QString("SAX fatal error - ") + 
         QString::fromUcs2(exception.getMessage()),50,this); 
  is_good=false;
}

bool
KludgeCfgReader::allOk()
{
  return is_good;
}

MemBufInputSource* 
KludgeCfgReader::getEmbeddedSchema()
{  
  if(schema_exp.isEmpty())  
    schema_exp=qUncompress(z_schema,sizeof(z_schema));    
    
  //for(int i=0;i<schema_exp.size();i++) printf("%c",schema_exp.data()[i]);
  
  return new MemBufInputSource((const unsigned char*)schema_exp.data(),
                               schema_exp.size(),"kludge_schema",false); 
}

QImage*
KludgeCfgReader::loadQImage(const KURL& kurl)
{
  logMsg(QString("QImage resource: ") + kurl.prettyURL(),0,this);
  QString tmpfile;  
  if(!KIO::NetAccess::download(kurl,tmpfile,(QWidget*)parent()))
  {
    logMsg(QString("Loading of ") + kurl.prettyURL() + 
           QString(" failed!"),50,this);
    return 0;
  }
  QImage *res=new QImage();
  res->load(tmpfile);          
  KIO::NetAccess::removeTempFile(tmpfile);
  if(res->isNull())
  {
    delete res;
    res=0;
  }
  if(!res) logMsg(QString("Resource loading failed. Check your config file!"),
                  50,this);
          
  return res;  
}

bool
KludgeMainWidget::loadXMLConfig(const KURL& kurl)
{
  bool res; 
  logMsg(QString("Starting configuration loading: ") + kurl.prettyURL(),0,this);  
  
  QString tmpfile;  
  if(!KIO::NetAccess::download(kurl,tmpfile,this))
  {
    logMsg(QString("Loading of ") + kurl.prettyURL() + 
           QString(" failed!"),50,this);
    return false;
  }
  
  res=parseXMLConfig(tmpfile);        
  
  KIO::NetAccess::removeTempFile(tmpfile);
  if(!res) logMsg(QString("XML parsing failed. Check your config file!"),50,this);
        
  return res;  
}

bool 
KludgeMainWidget::parseXMLConfig(const QString& file_name)
{
  bool res=true;
  XMLPlatformUtils::Initialize();  
  LocalFileInputSource *my_config;
  try
  {
    my_config = new LocalFileInputSource(file_name.ucs2());
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
  
  KludgeCfgReader *cfg_handler = new KludgeCfgReader(*this,this,"cparser");
  MemBufInputSource *my_schema = cfg_handler->getEmbeddedSchema();
  parser->setContentHandler(cfg_handler);  
  parser->setErrorHandler(cfg_handler);
  parser->loadGrammar(*my_schema,Grammar::SchemaGrammarType,true);
  connect(cfg_handler,SIGNAL(logMsg(const QString&,unsigned,const QObject*)),
          this,SLOT(logMsg(const QString&,unsigned,const QObject*)));  
    
  try
  {
    parser->parse(*my_config);
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
  res=cfg_handler->allOk();  
  disconnect(cfg_handler);  
  delete cfg_handler;
  delete my_schema;      
  delete my_config;  
  XMLPlatformUtils::Terminate();  
  return res;
}


#include "kludgecfgreader.moc"
