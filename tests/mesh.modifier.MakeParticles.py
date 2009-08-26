#python

import k3d
import testing

setup = testing.setup_mesh_modifier_test("PolyCube", "MakeParticles")

testing.mesh_comparison_to_reference(setup.document, setup.modifier.get_property("output_mesh"), "mesh.modifier.MakeParticles", 1)
