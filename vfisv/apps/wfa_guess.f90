PURE SUBROUTINE WFA_GUESS(OBS,LAMBDA_0,VELOCITY)
  ! J M Borrero
  ! Oct 28, 2007
  ! HAO-NCAR for HMI-Stanford
  USE CONS_PARAM
  USE INV_PARAM
  USE FILT_PARAM
  IMPLICIT NONE
 
  REAL(8), INTENT(IN)                       :: OBS(NBINS,4)
  REAL(8), INTENT(IN)                       :: LAMBDA_0
  REAL(DP), DIMENSION(NTUNE)                :: TUNE
  REAL(8), INTENT(OUT)                      :: VELOCITY
  INTEGER                                   :: L
  REAL(DP), DIMENSION(NBINS)                :: AVPROF
  !
  !--------------------------------------------
  ! Velocity: we use the COG method in Stokes I
  !--------------------------------------------
  AVPROF(:)=0D0
  DO L=1,NTUNE
     AVPROF(L)=MAXVAL(OBS(:,1))-OBS(L,1)
  ENDDO
!by RCE: temporarily changing TUNE
TUNE(1) = -170.0
TUNE(2) = -102.0
TUNE(3) = -34.4
TUNE(4) = 34.4
TUNE(5) = 102.0
TUNE(6) = 170.0 

  VELOCITY=SUM(AVPROF*TUNE)/SUM(AVPROF)
IF (MAXVAL(ABS(OBS(:,4))) .GT. 2D0*NOISE) VELOCITY = SUM(ABS(OBS(:,4))*TUNE)/SUM(ABS(OBS(:,4)))
  VELOCITY=1E-3*(2.998E10/LAMBDA_0)*VELOCITY    ! cm/s

END SUBROUTINE WFA_GUESS