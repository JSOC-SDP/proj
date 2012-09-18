!***********************************************************************************************************************************
! Module containing the field related data. 
!***********************************************************************************************************************************
module mgram_data
   real,dimension(:,:),allocatable :: dBzdz
   real,dimension(:,:),pointer :: Bx,By,Bz   ! Pointers to respective input arrays [_p suffix]
end module mgram_data
!***********************************************************************************************************************************