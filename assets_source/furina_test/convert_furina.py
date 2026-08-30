from pathlib import Path
import math
import sys

import bpy
from mathutils import Matrix, Vector


WORKSPACE = Path(__file__).resolve().parents[2]
ADDON_ROOT = (
    WORKSPACE
    / "assets_source"
    / "furina_test"
    / "blender_mmd_tools"
    / "blender_mmd_tools-main"
)
SOURCE_MODEL = (
    WORKSPACE
    / "assets_source"
    / "furina_test"
    / "official_mmd"
    / "【芙宁娜】.pmx"
)
OUTPUT_DIRECTORY = (
    WORKSPACE
    / "project"
    / "resources"
    / "test_models"
    / "furina"
)
OUTPUT_MODEL = OUTPUT_DIRECTORY / "furina.gltf"
OUTPUT_BLEND = (
    WORKSPACE
    / "assets_source"
    / "furina_test"
    / "furina_converted.blend"
)
STAGED_TEXTURE_DIRECTORY = (
    WORKSPACE
    / "assets_source"
    / "furina_test"
    / "converted_textures"
)


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def register_mmd_tools() -> None:
    opencc_path = WORKSPACE / "assets_source" / "furina_test" / "opencc_python"
    sys.path.insert(0, str(opencc_path))
    sys.path.insert(0, str(ADDON_ROOT))
    import mmd_tools

    mmd_tools.register()


def rename_export_assets() -> None:
    for index, obj in enumerate(bpy.data.objects):
        obj.name = f"furina_object_{index:03d}"
    for index, material in enumerate(bpy.data.materials):
        material.name = f"furina_material_{index:03d}"
    used_images = []
    for material in bpy.data.materials:
        if not material.use_nodes or not material.node_tree:
            continue
        for node in material.node_tree.nodes:
            if (
                node.type == "TEX_IMAGE"
                and node.image is not None
                and node.image not in used_images
            ):
                used_images.append(node.image)
    for index, image in enumerate(used_images):
        if not image.has_data:
            print(f"FURINA_TEXTURE_SKIPPED name={image.name}")
            continue
        texture_name = f"furina_texture_{index:03d}.png"
        image.name = texture_name
        image.filepath_raw = str(STAGED_TEXTURE_DIRECTORY / texture_name)
        image.file_format = "PNG"
        image.save()


def convert_materials_for_gltf() -> None:
    for index, material in enumerate(bpy.data.materials):
        base_image = None
        if material.use_nodes and material.node_tree:
            base_texture = material.node_tree.nodes.get("mmd_base_tex")
            if base_texture and base_texture.type == "TEX_IMAGE":
                base_image = base_texture.image

        diffuse = tuple(material.diffuse_color)
        if hasattr(material, "mmd_material"):
            mmd_material = material.mmd_material
            mmd_diffuse = tuple(mmd_material.diffuse_color)
            if len(mmd_diffuse) >= 3:
                diffuse = (
                    float(mmd_diffuse[0]),
                    float(mmd_diffuse[1]),
                    float(mmd_diffuse[2]),
                    float(mmd_material.alpha),
                )

        material.use_nodes = True
        nodes = material.node_tree.nodes
        links = material.node_tree.links
        nodes.clear()

        output = nodes.new("ShaderNodeOutputMaterial")
        output.location = (420.0, 0.0)
        shader = nodes.new("ShaderNodeBsdfPrincipled")
        shader.location = (80.0, 0.0)
        shader.inputs["Base Color"].default_value = diffuse
        shader.inputs["Alpha"].default_value = diffuse[3]
        shader.inputs["Roughness"].default_value = 0.65
        shader.inputs["Metallic"].default_value = 0.0
        links.new(shader.outputs["BSDF"], output.inputs["Surface"])

        if base_image is not None:
            base_image.name = f"furina_texture_{index:03d}"
            texture = nodes.new("ShaderNodeTexImage")
            texture.location = (-280.0, 0.0)
            texture.image = base_image
            links.new(texture.outputs["Color"], shader.inputs["Base Color"])
            links.new(texture.outputs["Alpha"], shader.inputs["Alpha"])

        material.diffuse_color = diffuse


def align_pose_bone_direction(
    pose_bone,
    target_direction: tuple[float, float, float],
) -> None:
    current_direction = pose_bone.tail - pose_bone.head
    if current_direction.length_squared < 0.000001:
        return

    rotation = current_direction.normalized().rotation_difference(
        Vector(target_direction).normalized()
    )
    pivot = pose_bone.head.copy()
    pose_bone.matrix = (
        Matrix.Translation(pivot)
        @ rotation.to_matrix().to_4x4()
        @ Matrix.Translation(-pivot)
        @ pose_bone.matrix
    )


def bake_static_flight_pose() -> None:
    armature = next(
        (obj for obj in bpy.data.objects if obj.type == "ARMATURE"),
        None,
    )
    if armature is None:
        return

    bpy.context.view_layer.objects.active = armature
    armature.select_set(True)
    bpy.context.view_layer.update()

    pose_targets = (
        ("腕.L", (0.48, 0.28, -0.83)),
        ("腕.R", (-0.48, 0.28, -0.83)),
        ("ひじ.L", (0.20, 0.24, -0.95)),
        ("ひじ.R", (-0.20, 0.24, -0.95)),
    )
    for bone_name, target_direction in pose_targets:
        pose_bone = armature.pose.bones.get(bone_name)
        if pose_bone is not None:
            align_pose_bone_direction(pose_bone, target_direction)
            bpy.context.view_layer.update()

    bpy.ops.object.mode_set(mode="OBJECT")
    mesh_objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    for mesh_object in mesh_objects:
        bpy.ops.object.select_all(action="DESELECT")
        mesh_object.select_set(True)
        bpy.context.view_layer.objects.active = mesh_object
        if mesh_object.data.shape_keys is not None:
            bpy.ops.object.shape_key_remove(all=True)
        for modifier in list(mesh_object.modifiers):
            if modifier.type == "ARMATURE":
                bpy.ops.object.modifier_apply(modifier=modifier.name)

    for mesh_object in mesh_objects:
        world_matrix = mesh_object.matrix_world.copy()
        mesh_object.parent = None
        mesh_object.matrix_world = (
            Matrix.Rotation(math.pi, 4, "Z") @ world_matrix
        )

    for obj in list(bpy.data.objects):
        if obj.type != "MESH":
            bpy.data.objects.remove(obj, do_unlink=True)


def print_model_summary() -> None:
    mesh_objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    armature_objects = [
        obj for obj in bpy.data.objects if obj.type == "ARMATURE"
    ]
    vertex_count = sum(len(obj.data.vertices) for obj in mesh_objects)
    triangle_count = sum(
        len(obj.data.loop_triangles) for obj in mesh_objects
    )
    print(
        "FURINA_IMPORT_OK "
        f"meshes={len(mesh_objects)} "
        f"armatures={len(armature_objects)} "
        f"vertices={vertex_count} "
        f"triangles={triangle_count}"
    )


def main() -> None:
    if not SOURCE_MODEL.is_file():
        raise FileNotFoundError(SOURCE_MODEL)

    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)
    STAGED_TEXTURE_DIRECTORY.mkdir(parents=True, exist_ok=True)
    for texture_path in STAGED_TEXTURE_DIRECTORY.glob("furina_texture_*.png"):
        texture_path.unlink()
    output_texture_directory = OUTPUT_DIRECTORY / "textures"
    output_texture_directory.mkdir(parents=True, exist_ok=True)
    for texture_path in output_texture_directory.iterdir():
        if texture_path.is_file():
            texture_path.unlink()
    clear_scene()
    register_mmd_tools()

    result = bpy.ops.mmd_tools.import_model(
        filepath=str(SOURCE_MODEL),
        types={"MESH", "ARMATURE"},
        scale=0.08,
        clean_model=True,
        remove_doubles=False,
        log_level="INFO",
    )
    if "FINISHED" not in result:
        raise RuntimeError(f"MMD import failed: {result}")

    bake_static_flight_pose()
    for obj in bpy.data.objects:
        if obj.type == "MESH":
            obj.data.calc_loop_triangles()

    print_model_summary()
    convert_materials_for_gltf()
    rename_export_assets()
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_BLEND))

    export_result = bpy.ops.export_scene.gltf(
        filepath=str(OUTPUT_MODEL),
        export_format="GLTF_SEPARATE",
        export_texture_dir="textures",
        export_texcoords=True,
        export_normals=True,
        export_materials="EXPORT",
        export_animations=False,
        export_skins=False,
        export_morph=False,
    )
    if "FINISHED" not in export_result:
        raise RuntimeError(f"glTF export failed: {export_result}")

    print(f"FURINA_EXPORT_OK path={OUTPUT_MODEL}")


if __name__ == "__main__":
    main()
