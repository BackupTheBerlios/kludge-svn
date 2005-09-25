/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <zlib.h>

int
main(int argc,char** argv)
{
  if(argc!=3) return 1;
  int fd=open(argv[2],O_RDONLY);
  if(-1==fd) return 2;
  struct stat fd_inf;
  if(-1==fstat(fd,&fd_inf) || fd_inf.st_size<1) return 3;
  unsigned char *rfb=mmap(0,fd_inf.st_size,PROT_READ,MAP_SHARED,fd,0);
    if(-1==(int)rfb) return 4;
  size_t cs=(fd_inf.st_size<400)?500:fd_inf.st_size+20+(fd_inf.st_size>>7);
  unsigned char *fb=malloc(cs);
  if(fb==0) return 5;
  unsigned long xlen=cs;
  if(Z_OK!=compress2(fb,&xlen,rfb,fd_inf.st_size,9)) return 6;
  
  int s1=xlen>>4; int s2=xlen-(s1<<4);
  if(s2==0) 
  {
    s1--;
    s2=16;
  }
  int p; unsigned char *q;
  printf("%s[%d] = {0x%02x, 0x%02x, 0x%02x, 0x%02x,\n",
         argv[1],xlen+4,(fd_inf.st_size>>24)&0xff,
	                (fd_inf.st_size>>16)&0xff,
		        (fd_inf.st_size>>8)&0xff,
		        (fd_inf.st_size)&0xff);
  q=fb;
  for(p=0;p<s1;p++)
  {
    printf("0x%02x, 0x%02x, 0x%02x, 0x%02x, ",q[0],q[1],q[2],q[3]);
    printf("0x%02x, 0x%02x, 0x%02x, 0x%02x, ",q[4],q[5],q[6],q[7]);
    printf("0x%02x, 0x%02x, 0x%02x, 0x%02x, ",q[8],q[9],q[10],q[11]);
    printf("0x%02x, 0x%02x, 0x%02x, 0x%02x,\n",q[12],q[13],q[14],q[15]);
    q+=16;
  }
  for(p=0;p<s2-1;p++)
    printf("0x%02x, ",q[p]);
 
  printf("0x%02x};\n",q[p]);
  return 0;
}
