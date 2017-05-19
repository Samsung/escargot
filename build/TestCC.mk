## build script for test/cctest/*
SRC_TC = $(foreach dir, test/cctest, $(wildcard $(dir)/*.cpp))
EXE_TC = $(SRC_TC:%.cpp= $(OUTDIR)/%.exe)
LINK_LIBS += -L$(OUTDIR) -Wl,-rpath=$(OUTDIR) -lescargot $(ESCARGOT_LDFLAGS_THIRD_PARTY)

x86.interpreter.debug.shared.cctest: x86.interpreter.debug.shared $(EXE_TC)
x86.interpreter.release.shared.cctest: x86.interpreter.release.shared $(EXE_TC)
x64.interpreter.debug.shared.cctest: x64.interpreter.debug.shared $(EXE_TC)
x64.interpreter.release.shared.cctest: x64.interpreter.release.shared $(EXE_TC)
x64.interpreter.debug.shared.cctest.run: x64.interpreter.debug.shared.cctest
	for f in $(EXE_TC); do LD_LIBRARY_PATH=. $$f; done;

$(OUTDIR)/%.exe: %.cpp $(DEPENDENCY_MAKEFILE) $(OUTDIR)/libescargot.so
	mkdir -p $(OUTDIR)/test/cctest
	$(QUIET) echo "[CCTEST] " $@
	$(QUIET) $(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DFLAGS) $(IFLAGS) $(LDFLAGS) $< -o $@ $(LINK_LIBS)
