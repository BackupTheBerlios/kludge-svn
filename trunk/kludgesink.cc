/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include "kludgesink"
#include <math.h>

KludgeSink::KludgeSink(QObject* parent, const char* name)
           :QObject(parent,name)
{
  src_names.setAutoDelete(true);
}

KludgeSink::~KludgeSink()
{
  
}

void 
KludgeSink::regSrc(const QString& gname, const QString& sname)
{  
  QMutexLocker src_names_is_safe(&src_names_lock);
  QStringList *snl=src_names[gname];
  
  if(!snl)      
  {
    snl=new QStringList;
    src_names.insert(gname,snl);    
  }    
  snl->append(sname);  
}

QStringList* 
KludgeSink::getPending(const QString& gname)
{
  QMutexLocker src_names_is_safe(&src_names_lock);
  return src_names[gname];
}

KludgeSinkGroup::KludgeSinkGroup(QObject* parent, const char* name)
                :QObject(parent,name)
{
  sink_hive.setAutoDelete(true);
}

void 
KludgeSinkGroup::makeConnections(KludgeSrcGroup& sgrp)
{
  QMutexLocker sink_hive_is_safe(&sink_hive_lock);
  QString sg_name(QString::fromUtf8(sgrp.name()));
  QDictIterator<KludgeSink> cs(sink_hive);
  QStringList *s_list; QStringList::iterator s_list_it;
  KludgeSource *t_src;
  
  for(;cs.current();++cs)
  {
    s_list=cs.current()->getPending(sg_name);    
    if(s_list && !s_list->isEmpty())
    {
      s_list_it=s_list->begin();
      while(s_list_it!=s_list->end())
      {
        t_src=sgrp.getSource(*s_list_it);
        if(t_src)
        {          
          connect(t_src,SIGNAL(send(const KludgeMsg&)),
                  cs.current(),SLOT(collector(const KludgeMsg&)));          
          s_list_it=s_list->remove(s_list_it);          
        }
        else
          ++s_list_it;
      }
    }  
  }  
}
  
void 
KludgeIMap::drawCmapBar(cairo_t* v_buf, unsigned num_stops, double width, 
                        double height)
{
  double cstep=1.0/(double)num_stops, istep=width*cstep, cpos=0.0;
  double cx, cy;
  cairo_get_current_point(v_buf,&cx,&cy);

  for(double cnt=0;cnt<(double)num_stops;cnt+=1.0)
  {
    cairo_set_source_rgb(v_buf,my_cmap.r(cstep*cnt,cur_cmap),
                        my_cmap.g(cstep*cnt,cur_cmap),
                        my_cmap.b(cstep*cnt,cur_cmap));
    cairo_rectangle(v_buf,cx+istep*cnt,cy,istep,height);
    cairo_fill(v_buf);    
  }
  
  cairo_move_to(v_buf,cx+2,cy-2);
  cairo_set_source_rgb(v_buf,my_cmap.r(0.0,cur_cmap),
                       my_cmap.g(0.0,cur_cmap),
                       my_cmap.b(0.0,cur_cmap));
  cairo_show_text(v_buf,(const char*)minval_s.utf8());    

  cairo_move_to(v_buf,cx+width-maxval_s_e.width-4,cy-2);
  cairo_set_source_rgb(v_buf,my_cmap.r(1.0,cur_cmap),
                       my_cmap.g(1.0,cur_cmap),
                       my_cmap.b(1.0,cur_cmap));  
  cairo_show_text(v_buf,(const char*)maxval_s.utf8());    
}

double 
KludgeIMap::normalize(double val)
{
  return gain*val+offset;
}

KludgeIMap::KludgeIMap(QObject* parent, const char* name) 
           :KludgeSinkGroup(parent,name)
{
  back_image=0;
  tmp_back_image=0;
  cmap_bar=0;
  cur_cmap=KludgeColorMap::JET;
  cairo_target=0;
  w=100.0; h=100.0;
  rx=0.0; ry=0.0;
  gain=1.0; offset=0.0;  
}

KludgeIMap::~KludgeIMap()
{
  cairoDetach();
}
  


void 
KludgeIMap::cairoAttach(QCairoFrame* cf)
{  
  if(!cf) return;
  if(!cairo_target) cairo_target=cf;
  if(tmp_back_image)
  {    
    back_image=cairo_target->createImage(*tmp_back_image);    
    delete tmp_back_image;
    tmp_back_image=0;    
  }
  cairo_target->appendItem(this);  
}

KludgeSink* 
KludgeIMap::getSink(const QString& name, const QDict<QString>* params)
{
  QMutexLocker sink_hive_is_safe(&sink_hive_lock);
  KludgeSink *res=sink_hive[name];
  if(res!=0)
  {  
    if(params!=0) res->setParams(*params);
  }  
  else if(res==0 && params!=0)
  {
    res=new Indicator(this,name.utf8());
    if(params!=0) res->setParams(*params);
    sink_hive.insert(name,res);
    drw_items.append(dynamic_cast<DrwItem*>(res));    
  }  
  return res;
}

void 
KludgeIMap::cairoDraw()
{
  DrwItem *cdrw;  
  if(cairo_target)
  {    
    cairo_t *v_buf=cairo_target->cairoTarget();
    cairo_select_font_face(v_buf,"Arial",CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(v_buf,15.0);
    cairo_text_extents(v_buf,(const char*)minval_s.utf8(),&minval_s_e);
    cairo_text_extents(v_buf,(const char*)maxval_s.utf8(),&maxval_s_e);
    
    cairo_move_to(v_buf,rx,ry);    
    if(back_image) 
    {
      cairo_set_source_surface(v_buf,back_image,0,0);
      cairo_paint(v_buf);
    }
    cairo_set_source_rgb(v_buf,0,0,0);
    cairo_rectangle(v_buf,rx,ry,w,h);
    cairo_stroke(v_buf);
    cairo_move_to(v_buf,rx,ry+h-10.0);
    drawCmapBar(v_buf,50,w,10.0); 
    sink_hive_lock.lock();
    for(cdrw=drw_items.first();cdrw;cdrw=drw_items.next())
    { 
      cairo_save(v_buf);
      cairo_move_to(v_buf,rx+cdrw->x(),ry+cdrw->y());
      cdrw->cairoDraw(v_buf);
      cairo_restore(v_buf);
    }
    sink_hive_lock.unlock();   
  }
}

void 
KludgeIMap::cairoDetach()
{
  if(cairo_target)
  {
    if(back_image) cairo_target->destroySurface(back_image);
    back_image=0;  
    cairo_target->removeItem(this);  
  }  
}

void 
KludgeIMap::setImage(QImage& qimg)
{  
  if(!qimg.isNull())
  {        
    if(cairo_target)
    {
      if(back_image) cairo_target->destroySurface(back_image);      
      back_image=cairo_target->createImage(qimg);
    }
    else
      tmp_back_image=&qimg;
    
    w=(double)qimg.width(); h=(double)qimg.height(); 
  }
}

void 
KludgeIMap::setParams(const QDict<QString>& params)
{
  QString *pval1, *pval2;
  /* Set colormap */
  if(0!=(pval1=params[QString("colormap")]))
  {
    cur_cmap=my_cmap.name2map((const char*)(*pval1));
  }
  /* Set min/max values */
  pval1=params[QString("maxval")];
  pval2=params[QString("minval")];
  minval=(pval2)?pval2->toDouble():0.0;
  maxval=(pval1)?pval1->toDouble():1.0;
  minval_s = QString("min: ") + QString::number(minval), 
  maxval_s = QString("max: ") + QString::number(maxval);
  double delta=maxval-minval;
  gain=(delta==0.0)?1.0:1.0/delta;
  offset=-gain*minval;    
  /* Set coordinates */
  pval1=params[QString("x")];
  pval2=params[QString("y")];
  double rx=(pval2)?pval2->toDouble():0.0;
  double ry=(pval1)?pval1->toDouble():0.0;      
}

double
KludgeIMap::x()
{
  return rx;
}

double
KludgeIMap::y()
{
  return ry;
}

KludgeIMap::Indicator::Indicator(KludgeIMap* parent, const char* name)
                      :KludgeSink(parent,name)
{
  my_group=parent;
  rx=0.0; ry=0.0; mval=0.0; data_offset=0;
}

KludgeIMap::Indicator::~Indicator()
{
  
}

void 
KludgeIMap::Indicator::setParams(const QDict<QString>& params)
{
  QString *pval;
  pval=params[QString("x")];
  rx=(pval)?pval->toDouble():0.0;
  pval=params[QString("y")];
  ry=(pval)?pval->toDouble():0.0;
  pval=params[QString("data_offset")];
  data_offset=(pval)?pval->toUInt():0;
}


void 
KludgeIMap::Indicator::cairoDraw(cairo_t* v_buf)
{  
  double cx, cy;
  cairo_get_current_point(v_buf,&cx,&cy);
  cairo_new_path(v_buf);
  cairo_move_to(v_buf,cx,cy-4.0);
  cairo_line_to(v_buf,cx+4.0,cy);
  cairo_line_to(v_buf,cx,cy+4.0);
  cairo_line_to(v_buf,cx-4.0,cy);
  cairo_close_path(v_buf);
  cairo_set_source_rgb(v_buf,
                       my_group->my_cmap.r(my_group->normalize(mval),
                                           my_group->cur_cmap),
                      my_group->my_cmap.g(my_group->normalize(mval),
                                          my_group->cur_cmap),
                      my_group->my_cmap.b(my_group->normalize(mval),
                                          my_group->cur_cmap));
  cairo_fill(v_buf);  
  cairo_new_path(v_buf);
  cairo_set_line_width(v_buf,1.0);
  cairo_move_to(v_buf,cx,cy-4.0);
  cairo_line_to(v_buf,cx+4.0,cy);
  cairo_line_to(v_buf,cx,cy+4.0);
  cairo_line_to(v_buf,cx-4.0,cy);
  cairo_close_path(v_buf);
  cairo_set_source_rgb(v_buf,0,0,0);
  cairo_stroke(v_buf);    
}

double 
KludgeIMap::Indicator::x()
{
  return rx;
}

double 
KludgeIMap::Indicator::y()
{
  return ry;
}
    
void
KludgeIMap::Indicator::collector(const KludgeMsg& msg)
{ 
  mval=msg.v[data_offset];
  my_group->cairo_target->update();  
}

#include "kludgesink.moc"
