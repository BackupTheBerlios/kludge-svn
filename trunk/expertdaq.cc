/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include "expertdaq"

#include <sys/time.h>

inline std_c::speed_t
getBaudValue(unsigned int sp)
{
  switch(sp)
  {
    case 75:     return B75;  
    case 150:    return B150; 
    case 300:    return B300; 
    case 600:    return B600; 
    case 1200:   return B1200;
    case 1800:   return B1800; 
    case 2400:   return B2400; 
    case 4800:   return B4800; 
    case 9600:   return B9600; 
    case 19200:  return B19200; 
    case 38400:  return B38400; 
    case 57600:  return B57600; 
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    default:
      return B0;
  }
}

ExpertDaq::ExpertDaq(QObject* parent, const char* name)
          :KludgeSrcGroup(parent,name)
{  
  go_on=false;
  dev_fd=0;  
  dev_speed=38400;  
  dev_name=QString("/dev/ttyUSB0");  
}

ExpertDaq::~ExpertDaq()
{
  passivate();
}

void
ExpertDaq::activate()
{
  if(!running())
  {
    dev_fd=fopen((const char*)dev_name,"r+");
    if(dev_fd==0)
    {
      logMsg(QString("Open of ") + dev_name + QString("failed"),100,this);
      return;
    }
    struct std_c::termios t_set;
    memset(&t_set,0,sizeof(t_set));
    
    std_c::speed_t ms=getBaudValue(dev_speed);    
    if(-1!=std_c::tcgetattr(fileno(dev_fd),&old_dev_params) && ms!=B0)
    {
      t_set.c_cflag = ms | CS8 | CLOCAL | CREAD ;
      t_set.c_iflag = IGNPAR;
      t_set.c_oflag = 0;
      t_set.c_lflag = 0;

      t_set.c_cc[VMIN]=0;
      t_set.c_cc[VTIME]=5; //500 msec      
      
      std_c::tcflush(fileno(dev_fd),TCIFLUSH);  
      if(-1!=std_c::tcsetattr(fileno(dev_fd),TCSANOW,&t_set))
      {
        go_on=true;
        start();
      }
      else
      {
        logMsg(QString("System error on device ") + dev_name,100,this);
        fclose(dev_fd);
        dev_fd=0;
      }
    }
    else
    {
      logMsg(QString("System error on device ") + dev_name,100,this);
      fclose(dev_fd);
      dev_fd=0;
    }    
  }
}

void
ExpertDaq::passivate()
{
  go_on=false;
  if(running()) wait();
}


KludgeSource*
ExpertDaq::newSource(const char* src_name)
{
  return new Ex9018p(this,src_name);
}

void
ExpertDaq::setParams(const QDict<QString>& params)
{
  QString *pval;
  if(0!=(pval=params["device"]))
    dev_name=*pval;
  if(0!=(pval=params["baudrate"]))
    dev_speed=pval->toUInt(0,10);
}

void
ExpertDaq::run()
{
  Ex9018p *cur_src;
  source_hive_lock.lock();
  QDictIterator<KludgeSource> src_list(source_hive);
  source_hive_lock.unlock();
  bool any_good;
  while(go_on)
  {
    source_hive_lock.lock();
    any_good=false;
    src_list.toFirst();
    while(0!=(cur_src=dynamic_cast<Ex9018p*>(src_list())))
    {
      if(cur_src->still_good)        
         cur_src->still_good=cur_src->update(dev_fd);
     
      if(cur_src->still_good) any_good=true;
      else 
      {
        logMsg(QString("Source ") + QString(cur_src->name()) + 
               QString(" failed"),50,this);
        source_hive.remove(QString::fromUtf8(cur_src->name()));
      }
    }    
    source_hive_lock.unlock();
    if(!any_good) break;
    std_c::sched_yield();
  }
  std_c::tcsetattr(fileno(dev_fd),TCSAFLUSH,&old_dev_params);
  fclose(dev_fd);
  dev_fd=0;
}

ExpertDaq::Ex9018p::Ex9018p(QObject* parent, const char* name)
                   :KludgeSource(parent,name)
{
  last_time.tv_sec=0; last_time.tv_usec=0;
  my_msg.v.resize(8);  
  my_msg.origin=this; 
  dev_addr=0;
  failure_counter=0;
  still_good=true;
}

ExpertDaq::Ex9018p::~Ex9018p()
{
}

void
ExpertDaq::Ex9018p::setParams(const QDict<QString>& params)
{
  QString *pval;
  
  pval=params[QString("address")];
  if(pval!=0) dev_addr=pval->toUInt(0,10);  
}

bool
ExpertDaq::Ex9018p::update(FILE* sfd)
{
  struct timezone czone;
  int rec_cnt;    
  
  rec_cnt=fprintf(sfd,"#%02X\r",dev_addr);  
  rec_cnt=fscanf(sfd,">%lf%lf%lf%lf%lf%lf%lf%lf\r",
                 &(my_msg.v[0]),&(my_msg.v[1]),
                 &(my_msg.v[2]),&(my_msg.v[3]),
                 &(my_msg.v[4]),&(my_msg.v[5]),
                 &(my_msg.v[6]),&(my_msg.v[7]));
  if(rec_cnt==8)  
  {    
    if(-1==gettimeofday(&last_time,&czone)) last_time.tv_usec+=1000;
    my_msg.t=last_time;
    send(my_msg);
    failure_counter=0;
    return true;        
  }  
  failure_counter++;  
  return (failure_counter<3); //allow three failed requests
}


#include "expertdaq.moc"
