SUBROUTINE median_sub(x, n, xmed)

  ! From Alan Miller, see http://jblevins.org/mirror/amiller/median.f90
  !====================================================================
  ! Find the median of X(1), ... , X(N), using as much of the quicksort
  ! algorithm as is needed to isolate it.
  ! N.B. On exit, the array X is partially ordered.

  !     Latest revision - 26 November 1996
  !     2017/08/01 - B.T. Welsch, edits: made reals *8, renamed median_sub
  IMPLICIT NONE
  
  INTEGER*8, INTENT(IN)                :: n
  REAL*8, INTENT(INOUT), DIMENSION(n) :: x
  REAL*8, INTENT(OUT)                  :: xmed
  
  ! Local variables
  
  REAL*8    :: temp, xhi, xlo, xmax, xmin
  LOGICAL :: odd
  INTEGER*8 :: hi, lo, nby2, nby2p1, mid, i, j, k
  
  nby2 = n / 2
  nby2p1 = nby2 + 1
  odd = .true.
  
  !     HI & LO are position limits encompassing the median.
  
  IF (n == 2 * nby2) odd = .false.
  lo = 1
  hi = n
  IF (n < 3) THEN
     IF (n < 1) THEN
        xmed = 0.0
        RETURN
     END IF
     xmed = x(1)
     IF (n == 1) RETURN
     xmed = 0.5*(xmed + x(2))
     RETURN
  END IF

  !     Find median of 1st, middle & last values.
  
10 mid = (lo + hi)/2
  xmed = x(mid)
  xlo = x(lo)
  xhi = x(hi)
  IF (xhi < xlo) THEN          ! Swap xhi & xlo
     temp = xhi
     xhi = xlo
     xlo = temp
  END IF
  IF (xmed > xhi) THEN
     xmed = xhi
  ELSE IF (xmed < xlo) THEN
     xmed = xlo
  END IF
  
  ! The basic quicksort algorithm to move all values <= the sort key (XMED)
  ! to the left-hand end, and all higher values to the other end.
  
  i = lo
  j = hi
50 DO
     IF (x(i) >= xmed) EXIT
     i = i + 1
  END DO
  DO
     IF (x(j) <= xmed) EXIT
     j = j - 1
  END DO
  IF (i < j) THEN
     temp = x(i)
     x(i) = x(j)
     x(j) = temp
     i = i + 1
     j = j - 1
     
     !     Decide which half the median is in.
     
     IF (i <= j) GO TO 50
  END IF
  
  IF (.NOT. odd) THEN
     IF (j == nby2 .AND. i == nby2p1) GO TO 130
     IF (j < nby2) lo = i
     IF (i > nby2p1) hi = j
     IF (i /= j) GO TO 100
     IF (i == nby2) lo = nby2
     IF (j == nby2p1) hi = nby2p1
  ELSE
     IF (j < nby2p1) lo = i
     IF (i > nby2p1) hi = j
     IF (i /= j) GO TO 100
     
     ! Test whether median has been isolated.
     
     IF (i == nby2p1) RETURN
  END IF
100 IF (lo < hi - 1) GO TO 10
  
  IF (.NOT. odd) THEN
     xmed = 0.5*(x(nby2) + x(nby2p1))
     RETURN
  END IF
  temp = x(lo)
  IF (temp > x(hi)) THEN
     x(lo) = x(hi)
     x(hi) = temp
  END IF
  xmed = x(nby2p1)
  RETURN
  
  ! Special case, N even, J = N/2 & I = J + 1, so the median is
  ! between the two halves of the series.   Find max. of the first
  ! half & min. of the second half, then average.
  
130 xmax = x(1)
  DO k = lo, j
     xmax = MAX(xmax, x(k))
  END DO
  xmin = x(n)
  DO k = i, hi
     xmin = MIN(xmin, x(k))
  END DO
  xmed = 0.5*(xmin + xmax)

  RETURN
END SUBROUTINE median_sub
