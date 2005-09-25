/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kpixmap.h>
#include <kiconloader.h>

#include "kludgemainwidget"


static const char description[] = I18N_NOOP("Kludge - Data logging and " \
                                             "presentation application");

static const char version[] = "0.1";

static KCmdLineOptions options[] =
{
  { "kludge_cfg <url>",I18N_NOOP("Load Kludge configuration file"),0 },
  KCmdLineLastOption
};

int 
main(int argc, char **argv)
{
  KAboutData about("kludge",I18N_NOOP("kludge"),version,description,
                   KAboutData::License_GPL,"(C) 2004 A. Dubov",0,0,             
                   "oakad@yahoo.com");
  about.addAuthor( "A. Dubov", 0, "oakad@yahoo.com" );
  KCmdLineArgs::init(argc, argv, &about);
  KCmdLineArgs::addCmdLineOptions( options );
  KApplication app;
    
  KludgeMainWidget *m_win=0;
    
  if (app.isRestored())
  {
    RESTORE(KludgeMainWidget);
  }
  else
  {
    // no session.. just start up normally
    KCmdLineArgs *args=KCmdLineArgs::parsedArgs();
    QCStringList kl_cfg=args->getOptionList("kludge_cfg");
        
    m_win = new KludgeMainWidget((kl_cfg.count()<1)?0:&kl_cfg[0]);
    m_win->resize(800,600);
    m_win->setMinimumSize(320,240);
    app.setMainWidget(m_win);
    //qDebug("App started\n");
    m_win->show();
    args->clear();
  }

  return app.exec();    	
}

KludgeMainWidget::KludgeMainWidget(QCString* init_cfg_url) 
                 :KMdiMainFrm(0,"kludge",KMdi::IDEAlMode)
{
  main_area=0;
  
  KIconLoader my_icons(QString("kludge"));
  KPixmap default_icon;
  
  main_area=new QCairoFrame(this,"gl data area");
  main_area_acc=createWrapper(main_area,QString("Data area"),QString("E1"));    
  msg_area=new KTextEdit(this);    
  msg_area->setReadOnly(true);
  msg_area->setTextFormat(PlainText);
  msg_area->setCaption(QString("Various Informal Messages"));
  msg_area->setIcon(my_icons.loadIcon(QString("openterm"),KIcon::Small));
  addWindow(main_area_acc);
  addToolWindow(msg_area,KDockWidget::DockBottom,getMainDockWidget(),30,
                QString("Various informal messages"),
                QString("Messages"));
  
  connect(main_area,SIGNAL(logMsg(const QString&,unsigned,const QObject*)),
          this,SLOT(logMsg(const QString&,unsigned,const QObject*)));
 
  file_dlg=new KFileDialog(QString("."),QString(),this,"F1",false);
  file_dlg->setCaption(QString("Available Files"));
  file_dlg->setIcon(my_icons.loadIcon(QString("fileopen"),KIcon::Small));
  file_dlg_acc=addToolWindow(file_dlg,KDockWidget::DockBottom,getMainDockWidget(),
                             30,QString("Available files"),
                             QString("Files"));  
  connect(file_dlg,SIGNAL(okClicked()),
          this,SLOT(setFile()));
     
  source_hive.setAutoDelete(true);
  sink_hive.setAutoDelete(true);    
  
  main_area->setFocus();  
  if(init_cfg_url)
    if(!loadXMLConfig(KURL(*init_cfg_url))) hubReset();    
  hubRun();
}

KludgeMainWidget::~KludgeMainWidget()
{
  hubReset();
}


void
KludgeMainWidget::hubRun()
{
  hubResolve();
  source_hive_lock.lock();
  QDictIterator<KludgeSrcGroup> srgrp(source_hive);
  for(;srgrp.current();++srgrp) srgrp.current()->activate();    
  source_hive_lock.unlock();    
}

void
KludgeMainWidget::hubReset()
{
  //Stop all source threads  
  source_hive_lock.lock();
  QDictIterator<KludgeSrcGroup> srgrp(source_hive);
  for(;srgrp.current();++srgrp) 
  {
    srgrp.current()->passivate();
    disconnect(srgrp.current());  
  }
  source_hive.clear();  
  source_hive_lock.unlock();  
  //Kick all sinks
  sink_hive_lock.lock();
  QDictIterator<KludgeSinkGroup> skgrp(sink_hive);
  for(;skgrp.current();++skgrp) disconnect(skgrp.current());
  sink_hive.clear();
  sink_hive_lock.unlock();  
}

QCairoFrame* 
KludgeMainWidget::hasGUI()
{
  return main_area;
}

void 
KludgeMainWidget::regSrcGroup(KludgeSrcGroup* kg)
{
  QMutexLocker source_hive_is_safe(&source_hive_lock);
  QString gname=QString::fromUtf8(kg->name());
  if(source_hive[gname])
  {
    logMsg(QString("Duplicate source group: ") + gname,50,this);
    delete kg;
  }
  else
  {
    source_hive.insert(gname,kg);  
    connect(kg,SIGNAL(logMsg(const QString&,unsigned,const QObject*)),
            this,SLOT(logMsg(const QString&,unsigned,const QObject*)));
  }
}

void
KludgeMainWidget::regSinkGroup(KludgeSinkGroup* kg)
{
  QMutexLocker sink_hive_is_safe(&sink_hive_lock);
  QString gname=QString::fromUtf8(kg->name());
  if(sink_hive[gname])
  {
    logMsg(QString("Duplicate sink group: ") + gname,50,this);
    delete kg;
  }
  else
  {
    sink_hive.insert(gname,kg);  
    connect(kg,SIGNAL(logMsg(const QString&,unsigned,const QObject*)),
            this,SLOT(logMsg(const QString&,unsigned,const QObject*)));
  }
}

KludgeSinkGroup* 
KludgeMainWidget::getSinkGroup(const QString& name)
{
  QMutexLocker sink_hive_is_safe(&sink_hive_lock);
  return sink_hive[name];
}

KludgeSrcGroup* 
KludgeMainWidget::getSrcGroup(const QString& name)
{
  QMutexLocker source_hive_is_safe(&source_hive_lock);
  return source_hive[name];
}

void
KludgeMainWidget::hubResolve()
{
  source_hive_lock.lock();
  sink_hive_lock.lock();
  if(!source_hive.isEmpty() && !sink_hive.isEmpty())
  {    
    QDictIterator<KludgeSrcGroup> cur_src_g(source_hive);
    QDictIterator<KludgeSinkGroup> cur_snk_g(sink_hive);
    
    for(;cur_src_g;++cur_src_g)
      for(cur_snk_g.toFirst();cur_snk_g;++cur_snk_g)
      {
         cur_snk_g.current()->makeConnections(*(cur_src_g.current()));     
         cur_src_g.current()->makeConnections(*(cur_snk_g.current()));
      }    
  }   
  sink_hive_lock.unlock();
  source_hive_lock.unlock();
}

void
KludgeMainWidget::logMsg(const QString& msg, unsigned level, 
                         const QObject* sender)
{
  QMutexLocker msg_log_is_safe(&msg_area_lock);
  QString sender_name=(!sender)?QString("unknown"):QString(sender->name());
  QString sender_class=(!sender)?QString("unknown"):QString(sender->className());
  sender_name=(sender_name.isEmpty())?QString("unnamed"):sender_name;
  sender_class=(sender_class.isEmpty())?QString("unnamed"):sender_class;
  QString mmsg=sender_class + QString("::") + sender_name + (" : ") + msg;
  
  switch(level)
  {
    case 100: 
      msg_area->setColor(QColor(255,0,0));
      msg_area->setBold(true);
      break;
    case 50:
      msg_area->setColor(QColor(255,0,0));
      msg_area->setBold(false);
      break;
    default:        
      msg_area->setColor(QColor(0,0,0));  
      msg_area->setBold(false);      
  }
  msg_area->append(mmsg);
}

void
KludgeMainWidget::setFile()
{
  file_dlg_acc->hide();
  hubReset();
  if(!loadXMLConfig(file_dlg->selectedURL())) hubReset();
  hubRun();
}

#include "kludgemainwidget.moc"
