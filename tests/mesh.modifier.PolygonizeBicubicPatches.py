#python

import testing

setup = testing.setup_mesh_modifier_test("Newell", "PolygonizeBicubicPatches")

setup.source.type = "teapot"
setup.modifier.subdivisions = 3

testing.mesh_comparison_to_reference(setup.document, setup.modifier.get_property("output_mesh"), "mesh.modifier.PolygonizeBicubicPatches", 1)
