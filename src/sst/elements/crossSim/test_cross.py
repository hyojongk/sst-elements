# Automatically generated SST Python input
import sst

# Define SST core options
sst.setProgramOption("timebase", "1 ps")
sst.setProgramOption("stopAtCycle", "25us")

# Define the simulation components
comp_c0_0 = sst.Component("c0.0", "crossSim.crossSimComponent")
comp_c0_0.addParams({
      "workPerCycle" : """1000""",
      "commSize" : """100""",
      "commFreq" : """1000"""
})



# Define the simulation links
link_s_0_10 = sst.Link("link_s_0_10")
link_s_0_10.connect( (comp_c0_0, "Nlink", "10000ps"), (comp_c0_0, "Slink", "10000ps") )
link_s_0_90 = sst.Link("link_s_0_90")
link_s_0_90.connect( (comp_c0_0, "Elink", "10000ps"), (comp_c0_0, "Wlink", "10000ps") )

# End of generated output.
