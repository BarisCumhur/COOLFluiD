IF ( CF_HAVE_LESMODELS )

LIST( APPEND SpectralFDLES_files
FaceDiffusiveFluxBR2ApproachLES.cxx
FaceDiffusiveFluxBR2ApproachLES.hh
FaceDiffusiveFluxIPApproachLES.cxx
FaceDiffusiveFluxIPApproachLES.hh
FaceDiffusiveFluxLocalApproachLES.cxx
FaceDiffusiveFluxLocalApproachLES.hh
LESBndFaceTermComputer.cxx
LESBndFaceTermComputer.hh
LESBR2BndFaceTermComputer.cxx
LESBR2BndFaceTermComputer.hh
LESBR2FaceTermComputer.cxx
LESBR2FaceTermComputer.hh
LESCompactVolTermComputer.cxx
LESCompactVolTermComputer.hh
LESFaceTermComputer.cxx
LESFaceTermComputer.hh
LESIPBndFaceTermComputer.cxx
LESIPBndFaceTermComputer.hh
LESIPFaceTermComputer.cxx
LESIPFaceTermComputer.hh
LESVolTermComputer.cxx
LESVolTermComputer.hh
SpectralFDLES.hh
)

LIST ( APPEND SpectralFDLES_cflibs SpectralFDNavierStokes LES)
CF_ADD_PLUGIN_LIBRARY ( SpectralFDLES )
CF_WARN_ORPHAN_FILES()

ENDIF ( CF_HAVE_LESMODELS )
