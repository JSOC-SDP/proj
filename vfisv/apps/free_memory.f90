SUBROUTINE FREE_MEMORY
  !
  ! J M Borrero
  ! Dec 16, 2009
  ! HAO-NCAR for HMI-Stanford
  !
  USE FILT_PARAM
  USE CONS_PARAM
  USE LINE_PARAM
  USE SVD_PARAM
  IMPLICIT NONE
  !
  DEALLOCATE(FILTER, TUNEPOS, FREELOC)
  DEALLOCATE(WAVE, HESS, DIVC)
  !
END SUBROUTINE FREE_MEMORY