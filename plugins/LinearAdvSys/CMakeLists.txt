LIST ( APPEND LinearAdvSys_files
ADSysTerm.cxx
ADSysTerm.hh
AdvectionDiffusionSysData.hh
AdvectionDiffusionSysPhysicalModel.ci
AdvectionDiffusionSysPhysicalModel.cxx
AdvectionDiffusionSysPhysicalModel.hh
AdvectionDiffusionSys2DPrim.cxx
AdvectionDiffusionSys2DPrim.hh
AdvectionDiffusionSys2DVarSet.cxx
AdvectionDiffusionSys2DVarSet.hh
AdvectionDiffusionSys3DPrim.cxx
AdvectionDiffusionSys3DPrim.hh
AdvectionDiffusionSys3DVarSet.cxx
AdvectionDiffusionSys3DVarSet.hh
AdvectionDiffusionSysVarSet.hh
LinearAdvSys.hh
LinearAdvSysPhysicalModel.hh
LinearAdvSysPhysicalModel.ci
LinearAdvSysPhysicalModel.cxx
LinearAdvSys2DPrim.hh
LinearAdvSys2DPrim.cxx
LinearAdvSys3DPrim.hh
LinearAdvSys3DPrim.cxx
LinearAdvSysTerm.hh
LinearAdvSysTerm.cxx
LinearAdvSysVarSet.hh
LinearAdvSys2DVarSet.hh
LinearAdvSys2DVarSet.cxx
LinearAdvSys3DVarSet.hh
LinearAdvSys3DVarSet.cxx
LinearAdvSys2DLinearPrim.hh
LinearAdvSys2DLinearPrim.cxx
LinearAdvSys3DLinearPrim.hh
LinearAdvSys3DLinearPrim.cxx
)


LIST ( APPEND LinearAdvSys_libs Framework )

CF_ADD_PLUGIN_LIBRARY ( LinearAdvSys )
CF_WARN_ORPHAN_FILES()
#ADD_SUBDIRECTORY ( testcases )
