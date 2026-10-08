
#---------------render_stub.s---------------
# Stands in for render.s in the CLI build. Ripes --mode cli has no LED matrix,
# so LED_MATRIX_0_BASE is undefined there and render.s cannot assemble.
# render_solution returns at once, so ida.s runs unchanged; the only cost is
# the call itself.

.text
render_solution:               # a0 = input string, a1 = solution length
    jalr x0, ra, 0
