! rmultipole: the even multipoles of the one-electron density of every level in a GRASP2018 run (physics-lab, native 0.47.2; MIT like GRASP).
! Built by tools/grasp/build.sh against GRASP2018's libraries (its angular routine ONEPARTICLEJJ, as rhfs uses it for the hyperfine operators).
! Input (stdin): name of the run (name.c, name.w, name.cm or name.m; isodata), then y if the mixing is from rci (name.cm), n for rmcdhf (name.m).
! Output name.mp (text): for every level and every even rank k <= min(2J, 2 lmax):
!   R_k(a,b) = sum_rs c_r c_s <CSF_r || T^k(a,b) || CSF_s> <kappa_a || C^k || kappa_b>   (k > 0: ONEPARTICLEJJ)
!   R_0(a,b) = -sum_rs c_r c_s ONESCALAR(r, s; a, b) = minus the one-body density matrix (as rdensity computes it)
! with T^k(a,b) the one-particle tensor coefficient of ONEPARTICLEJJ (GRASP's reduced-matrix-element convention) and <||C^k||> as rhfs's MATELT.
! tools/grasp/multipoles.py turns these into rho_k(r) of the M_J = J state (Wigner-Eckart) and into natural orbitals (k = 0); its tests fix the convention.
      PROGRAM RMULTIPOLE
      USE vast_kind_param, ONLY: DOUBLE
      USE parameter_def,   ONLY: NNNW
      USE default_C
      USE iounit_C
      USE eigv_C
      USE prnt_C
      USE orb_C
      USE syma_C
      USE getyn_I
      USE setdbg_I
      USE setmc_I
      USE setcon_I
      USE setcsla_I
      USE gethfd_I
      USE getmixblock_I
      USE factt_I
      USE itjpo_I
      USE ispar_I
      USE oneparticlejj_I
      USE onescalar_I
      USE clrx_I
      IMPLICIT NONE
      CHARACTER(LEN=24) :: NAME
      LOGICAL :: YES
      INTEGER :: NCI, NCORE_NOT_USED, IV, K, KTOP, IC, IR, IA, IB, I, J, LMAX, L, LOC, IOU, IRANK, NPROC, IOS
      CHARACTER(LEN=12) :: SUF
      REAL(DOUBLE) :: THR
      REAL(DOUBLE) :: TSHELL(NNNW), CC
      REAL(DOUBLE), ALLOCATABLE :: RKA(:,:,:,:), AMX(:)
      INTEGER, ALLOCATABLE :: KMX(:)
      NDEF = 0
      READ (*, '(A)') NAME
      YES = GETYN()
      ! optional third line "rank nproc" (0.47.4): this process takes the CSFs IC with MOD(IC, nproc) = rank and writes its partial sums, unrounded, to name.mp.<rank>;
      ! tools/grasp/run_two.py adds the parts up (the sum is the same as one process's, to rounding)
      IRANK = 0; NPROC = 1
      READ (*, *, IOSTAT=IOS) IRANK, NPROC
      IF (IOS /= 0) THEN
         IRANK = 0; NPROC = 1
      ENDIF
      NCI = 1
      IF (YES) NCI = 0
      CALL SETDBG
      CALL SETMC
      CALL SETCON
      CALL SETCSLA (NAME, NCORE_NOT_USED)
      CALL GETHFD (NAME)
      CALL GETMIXBLOCK (NAME, NCI)
      CALL FACTT
      LMAX = 0
      DO I = 1, NW
         L = NAK(I)
         IF (L < 0) L = -L - 1
         LMAX = MAX(LMAX, L)
      END DO
      ! 0.47.4: each CSF pair's angular coefficient is computed once and added into every level (it was computed again per level: Ca's 29,000-CSF block took 3 min per
      ! level); the sums, the thresholds and the output are as before (checked: identical name.mp on a finished block)
      ALLOCATE (RKA(NW, NW, NVEC, 0:2*LMAX), AMX(NCF), KMX(NVEC))
      RKA = 0.0D0
      DO IC = 1, NCF
         AMX(IC) = 0.0D0
         DO IV = 1, NVEC
            AMX(IC) = MAX(AMX(IC), ABS(EVEC(IC + (IV - 1)*NCF)))
         END DO
      END DO
      KTOP = 0
      DO IV = 1, NVEC
         KMX(IV) = MIN(IATJPO(IV) - 1, 2*LMAX)
         KTOP = MAX(KTOP, KMX(IV))
      END DO
      DO K = 0, KTOP, 2
         DO IC = 1, NCF
            IF (AMX(IC) < 1.0D-12) CYCLE
            IF (MOD(IC, NPROC) /= IRANK) CYCLE
            DO IR = 1, NCF
               IF (AMX(IR) < 1.0D-12) CYCLE
               IF (ITJPO(IC) /= ITJPO(IR) .OR. ISPAR(IC) /= ISPAR(IR)) CYCLE
               IF (K == 0) THEN
                  ! rank 0 as rdensity does it (ONESCALAR gives the density-matrix coefficient itself). ONEPARTICLEJJ with rank 0 gave nonzero coefficients
                  ! between CSFs that differ only in a subshell's J (Na 2p5 7g-2, J = 0 and 2), which a scalar cannot connect: the occupations then summed to 11.0009.
                  CALL ONESCALAR (IC, IR, IA, IB, TSHELL)
               ELSE
                  CALL ONEPARTICLEJJ (K, 1, IC, IR, IA, IB, TSHELL)
               ENDIF
               IF (IA == 0) CYCLE
               DO IV = 1, NVEC
                  IF (K > KMX(IV)) CYCLE
                  LOC = (IV - 1)*NCF
                  IF (ABS(EVEC(IC + LOC)) < 1.0D-12 .OR. ABS(EVEC(IR + LOC)) < 1.0D-12) CYCLE
                  CC = EVEC(IC + LOC)*EVEC(IR + LOC)
                  IF (K == 0) CC = -CC    ! R_0 = -(density matrix): the same sign as ONEPARTICLEJJ x <||C^0||> (= -occupation on the diagonal)
                  IF (IA == IB) THEN
                     DO I = 1, NW
                        IF (ABS(TSHELL(I)) <= 1.0D-12) CYCLE
                        RKA(I, I, IV, K) = RKA(I, I, IV, K) + CC*TSHELL(I)*CKS(I, K, I)
                     END DO
                  ELSE
                     IF (ABS(TSHELL(1)) > 1.0D-12) RKA(IA, IB, IV, K) = RKA(IA, IB, IV, K) + CC*TSHELL(1)*CKS(IA, K, IB)
                  ENDIF
               END DO
            END DO
         END DO
      END DO
      IOU = 41
      IF (NPROC > 1) THEN
         WRITE (SUF, '(I0)') IRANK
         OPEN (IOU, FILE=TRIM(NAME)//'.mp.'//TRIM(SUF), STATUS='UNKNOWN')
         THR = 0.0D0
      ELSE
         OPEN (IOU, FILE=TRIM(NAME)//'.mp', STATUS='UNKNOWN')
         THR = 1.0D-14
      ENDIF
      WRITE (IOU, '(A)') '# rmultipole: R_k(a,b) per level (see rmultipole.f90). orbitals: index n kappa'
      DO I = 1, NW
         WRITE (IOU, '(A,3I5)') 'orb', I, NP(I), NAK(I)
      END DO
      DO IV = 1, NVEC
         WRITE (IOU, '(A,I5,I5,I3,ES26.16)') 'level', IV, IATJPO(IV) - 1, IASPAR(IV), EVAL(IV) + EAV
         DO K = 0, KMX(IV), 2
            DO I = 1, NW
               DO J = 1, NW
                  IF (ABS(RKA(I, J, IV, K)) > THR) WRITE (IOU, '(A,I3,2I5,ES26.16)') 'R', K, I, J, RKA(I, J, IV, K)
               END DO
            END DO
         END DO
      END DO
      CLOSE (IOU)
      WRITE (ISTDE, *) 'RMULTIPOLE: Execution complete.'
      CONTAINS
      REAL(DOUBLE) FUNCTION CKS (I1, KK, I2)    ! the angular factor: <||C^k||> for k > 0; 1 for k = 0 (ONESCALAR's coefficient is already the density matrix)
      INTEGER, INTENT(IN) :: I1, KK, I2
      CKS = 1.0D0
      IF (KK > 0) CKS = CK(I1, KK, I2)
      END FUNCTION CKS
      ! <kappa_a || C^k || kappa_b>, as rhfs's MATELT writes it for an even rank (0 when l_a + k + l_b is odd)
      REAL(DOUBLE) FUNCTION CK (I1, KK, I2)
      INTEGER, INTENT(IN) :: I1, KK, I2
      INTEGER :: KAP1, KAP2, L1, L2
      KAP1 = NAK(I1); KAP2 = NAK(I2)
      L1 = KAP1; IF (KAP1 < 0) L1 = -KAP1 - 1
      L2 = KAP2; IF (KAP2 < 0) L2 = -KAP2 - 1
      CK = 0.0D0
      IF (MOD(L1 + KK + L2, 2) /= 0) RETURN
      CK = SQRT(DBLE(2*ABS(KAP2)))*CLRX(KAP1, KK, KAP2)
      IF (MOD(KAP1 + 1, 2) /= 0) CK = -CK
      END FUNCTION CK
      END PROGRAM RMULTIPOLE
