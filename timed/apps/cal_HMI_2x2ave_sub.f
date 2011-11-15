      SUBROUTINE MAINSUB1 (velo, wid, num_annuli, time, coef_temp,
     +  n_start, n_th, a, pix_locat, len2, output, output2)
C pix_locat is the filename for pixel locations.
C a is the 256x256x512 datacube for analysis.
C output is the final results of travel times.
      IMPLICIT REAL*4(a-h,o-z)

      PARAMETER (n1=512,n2=512,n3=640,n12=n1/2,n22=n2/2)
      PARAMETER (num=199,n_cor=20,n_coe=5,nca=8,num_ann_max=20)
C set one num_ann_max as the upper limits of num_annuli so that to declare
C a few arrays.
      PARAMETER (spat_res=0.06)
      PARAMETER (tol=1.E-4,PI=3.14159)
      DIMENSION a(n1,n2,n3),c(n3,n2,n1)
      DIMENSION we_out(n1/2,n2/2),we_in(n1/2,n2/2),sn_out(n1/2,n2/2)
      DIMENSION oi_out(n1/2,n2/2),oi_in(n1/2,n2/2),sn_in(n1/2,n2/2)
      DIMENSION output(n1/2,n2/2,4),output2(n1/2,n2/2,4)
      DIMENSION rr_we(num,n2/2,n1/2),rr_ns(num,n2/2,n1/2)
      DIMENSION gb(n1/2,n2/2,2),rr_oi(num,n2/2,n1/2)
      DIMENSION r_ns(num),r_we(num),r_oi(num)
      DIMENSION rn_tot(num),rs_tot(num),rw_tot(num),re_tot(num)
      DIMENSION r(num),rn(num),rs(num),rw(num),re(num)
      DIMENSION shft(num_ann_max),time(num_ann_max),lag(num),ia(n_coe)
      DIMENSION ori(n3),des_n(n3),des_s(n3),des_w(n3),des_e(n3)
      DIMENSION x(n_cor),y(n_cor),coef(n_coe),coef_temp(n_coe)
      DIMENSION temp1(n1,n2),temp2(n1,n2),t1(2),t2(2)
      INTEGER*2 num_w,num_e,num_n,num_s
      INTEGER*2 quad_w_x(100),quad_w_y(100),quad_e_x(100),quad_e_y(100)
      INTEGER*2 quad_n_x(100),quad_n_y(100),quad_s_x(100),quad_s_y(100)
      INTEGER np,naxes(3),noutaxes(3),num_annuli
      REAL velo,wid
      INTEGER n_start, n_th
      CHARACTER pix_locat*len2

      DATA naxes/n1,n2,n3/
      DATA noutaxes/n12,n22,4/
      DATA ia/1,1,1,1,1/

C      OPEN(3,FILE=para_fil,STATUS='OLD')
C      READ(3,*) velo,wid,num_annuli
C      READ(3,*) (time(i),i=1,num_annuli)
C      READ(3,*) (coef_temp(i),i=1,n_coe)
C      READ(3,*) n_start
C      READ(3,*) n_th
C      CLOSE(3)
      DO 1 i=1,num_annuli
        shft(i)=time(num_annuli/2+1)-time(i)
 1    CONTINUE

      CALL FILTER(a,n1,n2,n3,velo,wid,spat_res)
      DO 2 k=1,n3
        DO 2 j=1,n2
          DO 2 i=1,n1
            c(k,j,i)=a(i,j,k)/n1/n2/n3
 2    CONTINUE

C      WRITE(*,*) 'Now calculations begin ...'
      DO 5 i=1,num
        lag(i)=i-(num+1)/2
 5    CONTINUE
      OPEN(3,FILE=pix_locat,STATUS='OLD',FORM='UNFORMATTED')
      e=ETIME(t1)
      DO 10 i=1,n1/2
        itemp=2*i
        DO 15 j=1,n2/2
          jtemp=2*j
          CALL SSCAL(num,0.,rw_tot,1)
          CALL SSCAL(num,0.,re_tot,1)
          CALL SSCAL(num,0.,rn_tot,1)
          CALL SSCAL(num,0.,rs_tot,1)

C this is a special case of reading in locations, when I use circles.
C for other more general cases, need to revise this part.
          DO 20 m=1,num_annuli
            CALL SSCAL(num,0.,rw,1)
            CALL SSCAL(num,0.,re,1)
            CALL SSCAL(num,0.,rn,1)
            CALL SSCAL(num,0.,rs,1)
            DO 25 ii=itemp-1,itemp
              DO 25 jj=jtemp-1,jtemp
                READ(3) num_w,num_e,num_n,num_s
                READ(3) quad_w_x(1:num_w+1)
                READ(3) quad_w_y(1:num_w+1)
                READ(3) quad_e_x(1:num_e+1)
                READ(3) quad_e_y(1:num_e+1)
                READ(3) quad_n_x(1:num_n+1)
                READ(3) quad_n_y(1:num_n+1)
                READ(3) quad_s_x(1:num_s+1)
                READ(3) quad_s_y(1:num_s+1)

                CALL SSCAL(n3,0.,des_w,1)
                CALL SSCAL(n3,0.,des_e,1)
                CALL SSCAL(n3,0.,des_n,1)
                CALL SSCAL(n3,0.,des_s,1)

                CALL SCOPY(n3,c(1,jj,ii),1,ori,1)
                DO 31 k=1,num_w
                  nxx=quad_w_x(k)+1
                  nyy=quad_w_y(k)+1
                  IF((nxx.GE.1).AND.(nyy.GE.1).AND.(nxx.LE.n1).AND.
     +              (nyy.LE.n2))
     +              CALL SAXPY(n3,1.,c(1,nyy,nxx),1,des_w,1)
 31             CONTINUE
                DO 32 k=1,num_e
                  nxx=quad_e_x(k)+1
                  nyy=quad_e_y(k)+1
                  IF((nxx.GE.1).AND.(nyy.GE.1).AND.(nxx.LE.n1).AND.
     +              (nyy.LE.n2))
     +              CALL SAXPY(n3,1.,c(1,nyy,nxx),1,des_e,1)
 32             CONTINUE
                DO 33 k=1,num_n
                  nxx=quad_n_x(k)+1
                  nyy=quad_n_y(k)+1
                  IF((nxx.GE.1).AND.(nyy.GE.1).AND.(nxx.LE.n1).AND.
     +              (nyy.LE.n2))
     +              CALL SAXPY(n3,1.,c(1,nyy,nxx),1,des_n,1)
 33             CONTINUE
                DO 34 k=1,num_s
                  nxx=quad_s_x(k)+1
                  nyy=quad_s_y(k)+1
                  IF((nxx.GE.1).AND.(nyy.GE.1).AND.(nxx.LE.n1).AND.
     +              (nyy.LE.n2))
     +              CALL SAXPY(n3,1.,c(1,nyy,nxx),1,des_s,1)
 34             CONTINUE

                CALL C_CORRELATE(ori,des_w,n3,lag,num,r)
                CALL SAXPY(num,1.,r,1,rw,1)
                CALL C_CORRELATE(ori,des_e,n3,lag,num,r)
                CALL SAXPY(num,1.,r,1,re,1)
                CALL C_CORRELATE(ori,des_n,n3,lag,num,r)
                CALL SAXPY(num,1.,r,1,rn,1)
                CALL C_CORRELATE(ori,des_s,n3,lag,num,r)
                CALL SAXPY(num,1.,r,1,rs,1)
 25         CONTINUE

            CALL SHIFT(rw,num,shft(m))
            CALL SHIFT(re,num,shft(m))
            CALL SHIFT(rn,num,shft(m))
            CALL SHIFT(rs,num,shft(m))
            DO 40 ll=1,num
              rw_tot(ll)=rw_tot(ll)+rw(ll)
              re_tot(ll)=re_tot(ll)+re(ll)
              rn_tot(ll)=rn_tot(ll)+rn(ll)
              rs_tot(ll)=rs_tot(ll)+rs(ll)
 40         CONTINUE
 20       CONTINUE 

          DO 45 ll=1,num
            r_ns(ll)=rn_tot(ll)+rs_tot(num-ll+1)
            r_we(ll)=rw_tot(ll)+re_tot(num-ll+1)
            r_oi(ll)=rn_tot(ll)+rs_tot(ll)+re_tot(ll)+rw_tot(ll)
            rr_oi(ll,j,i)=r_oi(ll)
            rr_we(ll,j,i)=r_we(ll)
            rr_ns(ll,j,i)=r_ns(ll)
 45       CONTINUE

 15     CONTINUE
        e=ETIME(t2)
C        IF((i MOD 10).EQ.0) WRITE(*,*) i,(t2(1)-t1(1))/60.
 10   CONTINUE
      CLOSE(3)

      CALL DO_FITTING(rr_oi,n1/2,n2/2,num,n_cor,n_start,coef_temp,n_coe,
     +                ia,oi_out,oi_in)
      CALL DO_FITTING(rr_we,n1/2,n2/2,num,n_cor,n_start,coef_temp,n_coe,
     +                ia,we_out,we_in)
      CALL DO_FITTING(rr_ns,n1/2,n2/2,num,n_cor,n_start,coef_temp,n_coe,
     +                ia,sn_out,sn_in)
      
      DO 100 i=1,n1/2
        DO 100 j=1,n2/2
          output(i,j,1)=(0.5*(oi_out(i,j)+oi_in(i,j))-coef_temp(5))*0.75
          output(i,j,2)=(oi_out(i,j)-oi_in(i,j))*0.75
          output(i,j,3)=(we_out(i,j)-we_in(i,j))*0.75
          output(i,j,4)=(sn_out(i,j)-sn_in(i,j))*0.75
 100  CONTINUE

      CALL GBTIMES02(rr_oi,n12,n22,num,n_th,gb)
      DO 101 i=1,n1/2
        DO 101 j=1,n2/2
          output2(i,j,1)=gb(i,j,1)/60.
          output2(i,j,2)=gb(i,j,2)/60.
 101  CONTINUE
      CALL GBTIMES02(rr_we,n12,n22,num,n_th,gb)
      DO 102 i=1,n1/2
        DO 102 j=1,n2/2
          output2(i,j,3)=gb(i,j,2)/60.
 102  CONTINUE
      CALL GBTIMES02(rr_ns,n12,n22,num,n_th,gb)
      DO 103 i=1,n1/2
        DO 103 j=1,n2/2
          output2(i,j,4)=gb(i,j,2)/60.
 103  CONTINUE

      END