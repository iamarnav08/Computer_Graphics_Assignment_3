#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <cstring>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <SFML/Graphics.hpp>
#include "math_utils.h"

struct Ray {
    Vector3f origin;
    Vector3f direction;
};

Ray ray_create(const Vector3f& origin, const Vector3f& direction) {
    Ray ray;
    ray.origin = origin;
    ray.direction = direction.Normalize();
    return ray;
}

Vector3f ray_at(const Ray& ray, double t) {
    return ray.origin + ray.direction * t;
}

struct Material {
    Vector3f color;
    double ambient;
    double diffuse;
    double specular;
    double shininess;
    double reflectivity;
};

Material material_create() {
    Material mat;
    mat.color = Vector3f(1, 1, 1);
    mat.ambient = 0.1;
    mat.diffuse = 0.7;
    mat.specular = 0.3;
    mat.shininess = 10;
    mat.reflectivity = 10;
    return mat;
}

Material material_create_custom(const Vector3f& color, double ambient, double diffuse, double specular, double shininess, double reflectivity){
    Material mat;
    mat.color = color;
    mat.ambient = ambient;
    mat.diffuse = diffuse;
    mat.specular = specular;
    mat.shininess = shininess;
    mat.reflectivity = reflectivity;
    return mat;
}

struct HitRecord {
    Vector3f point;
    Vector3f normal;
    double t;
    bool front_face;
    Material material;
};

void hit_record_set_face_normal(HitRecord& rec, const Ray& ray, const Vector3f& outward_normal) {
    rec.front_face = ray.direction.Dot(outward_normal) < 0;
    rec.normal = rec.front_face ? outward_normal : outward_normal * -1.0f;
}

typedef struct Triangle {
    Vector3f v0, v1, v2;
    Vector3f normal;
    Material material;
}Triangle;

Triangle triangle_create(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, const Material& mat) {
    Triangle triangle;
    triangle.v0 = v0;
    triangle.v1 = v1;
    triangle.v2 = v2;
    triangle.material = mat;
    
    Vector3f edge1 = v1-v0;
    Vector3f edge2 = v2-v0;
    triangle.normal = (edge1.Cross(edge2)).Normalize();
    
    return triangle;
}

bool triangle_hit(const Triangle& triangle, const Ray& ray, double t_min, double t_max, HitRecord& rec) {
    double denom = triangle.normal.Dot(ray.direction);
    
    if (std::abs(denom) < 1e-8)
        return false;
        
    double t = (triangle.normal.Dot(triangle.v0 - ray.origin)) / denom;
    
    if (t < t_min || t > t_max)
        return false;
        
    Vector3f p = ray_at(ray, t);
    
    Vector3f v0v1 = triangle.v1 - triangle.v0;
    Vector3f v0v2 = triangle.v2 - triangle.v0;
    Vector3f v0p = p - triangle.v0;  
    
    double d00 = v0v1.Dot(v0v1);
    double d01 = v0v1.Dot(v0v2);
    double d11 = v0v2.Dot(v0v2);
    double d20 = v0p.Dot(v0v1);
    double d21 = v0p.Dot(v0v2);
    
    double denom_inv = 1.0 / (d00 * d11 - d01 * d01);
    double v = (d11 * d20 - d01 * d21) * denom_inv;
    double w = (d00 * d21 - d01 * d20) * denom_inv;
    double u = 1.0 - v - w;
    
    if (u < 0 || v < 0 || w < 0 || u > 1 || v > 1 || w > 1) return false;
    
    rec.t = t;
    rec.point = p;
    rec.material = triangle.material;
    hit_record_set_face_normal(rec, ray, triangle.normal);
    
    return true;
}

typedef struct Sphere {
    Vector3f center;
    double radius;
    Material material;
}Sphere;

Sphere sphere_create(const Vector3f& center, double radius, const Material& material) {
    Sphere sphere;
    sphere.center = center;
    sphere.radius = radius;
    sphere.material = material;
    return sphere;
}

bool sphere_hit(const Sphere& sphere, const Ray& ray, double t_min, double t_max, HitRecord& rec) {
    Vector3f oc = ray.origin - sphere.center; // (p-c)
    double a = ray.direction.Dot(ray.direction); // (d.d)
    double half_b = oc.Dot(ray.direction);  // d.(p-c)
    double c = oc.Dot(oc) - sphere.radius * sphere.radius;  // (p-c).(p-c) - r^2
    double discriminant = half_b * half_b - a * c;

    if (discriminant < 0)
        return false;

    double sqrtd = std::sqrt(discriminant);

    // Finding closest root in range
    double root = (-half_b - sqrtd) / a;
    if (root < t_min || t_max < root) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || t_max < root)
            return false;
    }

    rec.t = root;
    rec.point = ray_at(ray, rec.t);
    Vector3f outward_normal = (rec.point - sphere.center) * (1.0/sphere.radius);
    hit_record_set_face_normal(rec, ray, outward_normal);
    rec.material = sphere.material;

    return true;
}

typedef struct Mesh {
    std::vector<Triangle> triangles;
    Material material;
}Mesh;

Mesh mesh_create() {
    Mesh mesh;
    return mesh;
}

void mesh_add_triangle(Mesh& mesh, const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, const Material& material) {
    mesh.triangles.push_back(triangle_create(v0, v1, v2, material));
}

bool mesh_hit(const Mesh& mesh, const Ray& ray, double t_min, double t_max, HitRecord& rec) {
    HitRecord temp_rec;
    bool hit_anything = false;
    double closest_so_far = t_max;

    for (const auto& triangle : mesh.triangles) {
        if (triangle_hit(triangle, ray, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }
    return hit_anything;
}

struct Scene {
    std::vector<std::shared_ptr<Sphere>> spheres;
    std::vector<std::shared_ptr<Triangle>> triangles;
    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<Vector3f> lights;
};

Scene scene_create() {
    Scene scene;
    return scene;
}

void scene_add_sphere(Scene& scene, const Sphere& sphere) {
    scene.spheres.push_back(std::make_shared<Sphere>(sphere));
}

void scene_add_triangle(Scene& scene, const Triangle& triangle) {
    scene.triangles.push_back(std::make_shared<Triangle>(triangle));
}

void scene_add_mesh(Scene& scene, const Mesh& mesh) {
    scene.meshes.push_back(std::make_shared<Mesh>(mesh));
}

void scene_add_light(Scene& scene, const Vector3f& light) {
    scene.lights.push_back(light);
}

bool scene_hit(const Scene& scene, const Ray& ray, double t_min, double t_max, HitRecord& rec) {
    HitRecord temp_rec;
    bool hit_anything = false;
    double closest_so_far = t_max;

    for (const auto& sphere : scene.spheres) {
        if (sphere_hit(*sphere, ray, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    for (const auto& triangle : scene.triangles) {
        if (triangle_hit(*triangle, ray, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    for (const auto& mesh : scene.meshes) {
        if (mesh_hit(*mesh, ray, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    return hit_anything;
}

bool scene_is_shadowed(const Scene& scene, const Vector3f& point, const Vector3f& light_pos) {
    Vector3f direction = light_pos - point;
    float distance = direction.length();
    Ray shadow_ray = ray_create(
        point + direction.Normalize() * 0.001f, 
        direction.Normalize()
    );
    
    HitRecord rec;
    return scene_hit(scene, shadow_ray, 0.001, distance - 0.001, rec);
}

struct Camera {
    Vector3f origin;
    Vector3f lower_left_corner;
    Vector3f horizontal;
    Vector3f vertical;
};

Camera camera_create( Vector3f lookfrom, Vector3f lookat, Vector3f vup, float vfov, float aspect_ratio){
    Camera camera;
    
    float theta = vfov * M_PI / 180.0f;
    float h = tan(theta / 2);
    float viewport_height = 2.0f * h;
    float viewport_width = aspect_ratio * viewport_height;

    Vector3f w = (lookfrom - lookat).Normalize();
    Vector3f u = (vup.Cross(w)).Normalize();
    Vector3f v = w.Cross(u);

    camera.origin = lookfrom;
    camera.horizontal = u * viewport_width;
    camera.vertical = v * viewport_height;
    camera.lower_left_corner = camera.origin - camera.horizontal * 0.5f - camera.vertical * 0.5f - w;
    
    return camera;
}


Ray camera_get_ray(const Camera& camera, float s, float t) {
    return ray_create(
        camera.origin,
        camera.lower_left_corner + camera.horizontal * s + camera.vertical * t - camera.origin
    );
}



// Function to save the rendered image as a PPM file
void save_ppm(const std::vector<Vector3f>& pixels, int width, int height, const std::string& filename) {
    std::ofstream outfile(filename);
    outfile << "P3\n" << width << ' ' << height << "\n255\n";
    
    for (const auto& pixel : pixels) {
        float r = std::sqrt(std::clamp(pixel.x, 0.0f, 1.0f));
        float g = std::sqrt(std::clamp(pixel.y, 0.0f, 1.0f));
        float b = std::sqrt(std::clamp(pixel.z, 0.0f, 1.0f));
        
        int ir = static_cast<int>(255.99f * r);
        int ig = static_cast<int>(255.99f * g);
        int ib = static_cast<int>(255.99f * b);
        
        outfile << ir << ' ' << ig << ' ' << ib << '\n';
    }
    
    outfile.close();
}


Vector3f ray_color(const Ray& ray, const Scene& scene, int depth) {
    if (depth <= 0) {
        return Vector3f(0, 0, 0);
    }

    HitRecord rec;
    if (scene_hit(scene, ray, 0.001, std::numeric_limits<double>::infinity(), rec)) {
        Vector3f ambient = rec.material.color * rec.material.ambient;
        Vector3f diffuse_specular(0, 0, 0);

        for (const auto& light : scene.lights) {
            if (scene_is_shadowed(scene, rec.point, light)) {
                continue;
            }

            Vector3f light_dir = (light - rec.point).Normalize();
            Vector3f view_dir = (ray.origin - rec.point).Normalize();
            
            float dot_prod = rec.normal.Dot(light_dir);
            Vector3f reflect_dir = rec.normal * (2.0f * dot_prod) - light_dir;
            
            float diff = std::max(dot_prod, 0.0f);
            Vector3f diffuse = rec.material.color * rec.material.diffuse * diff;
            
            float spec = std::pow(std::max(view_dir.Dot(reflect_dir), 0.0f), rec.material.shininess);
            Vector3f specular = Vector3f(1, 1, 1) * (rec.material.specular * spec);
            
            diffuse_specular += (diffuse + specular);
        }

        Vector3f result = ambient + diffuse_specular;
        
        if (rec.material.reflectivity > 0.0f) {
            float dot_prod = ray.direction.Dot(rec.normal);
            Vector3f reflected = ray.direction - rec.normal * (2.0f * dot_prod);
            
            Ray reflect_ray = ray_create(
                rec.point + rec.normal * 0.001f, 
                reflected
            );
            
            Vector3f reflected_color = ray_color(reflect_ray, scene, depth - 1);
            result = result * (1.0f - rec.material.reflectivity) + reflected_color * rec.material.reflectivity;
        }
        
        return result;
    }

    Vector3f unit_direction = ray.direction.Normalize();
    float t = 0.5f * (unit_direction.y + 1.0f);
    return Vector3f(1.0f, 1.0f, 1.0f) * (1.0f - t) + Vector3f(0.5f, 0.7f, 1.0f) * t;
}


std::shared_ptr<Mesh> load_obj_file(const std::string& filename, const Material& material) {
    std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>(mesh_create());
    mesh->material = material;
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open OBJ file: " << filename << std::endl;
        exit(1);
    }
    
    std::cout << "Loading OBJ file: " << filename << std::endl;
    
    std::vector<Vector3f> vertices;
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;
            
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        
        if (token == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            vertices.push_back(Vector3f(x, y, z));
        }
    }
    
    if (vertices.empty()) {
        std::cerr << "Error: No vertices found in " << filename << std::endl;
        exit(1);
    }
    
    Vector3f min_bounds(std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max());
    Vector3f max_bounds(std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest());
                    
    for (const auto& v : vertices) {
        min_bounds.x = std::min(min_bounds.x, v.x);
        min_bounds.y = std::min(min_bounds.y, v.y);
        min_bounds.z = std::min(min_bounds.z, v.z);
        
        max_bounds.x = std::max(max_bounds.x, v.x);
        max_bounds.y = std::max(max_bounds.y, v.y);
        max_bounds.z = std::max(max_bounds.z, v.z);
    }
    
    Vector3f center = (min_bounds + max_bounds) * 0.5f;
    Vector3f dimensions = max_bounds - min_bounds;
    float max_dimension = std::max(dimensions.x, std::max(dimensions.y, dimensions.z));
    float scale = 2.0f / max_dimension; 
    
    std::cout << "Mesh bounds: (" << min_bounds.x << ", " << min_bounds.y << ", " << min_bounds.z 
              << ") to (" << max_bounds.x << ", " << max_bounds.y << ", " << max_bounds.z << ")" << std::endl;
    std::cout << "Center: (" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;
    std::cout << "Scale: " << scale << std::endl;
    
    file.clear();
    file.seekg(0);
    
    int face_count = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;
            
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        
        if (token == "f") {
            std::vector<int> face_vertices;
            
            while (iss >> token) {
                std::string vertex_index = token.substr(0, token.find('/'));
                if (!vertex_index.empty()) {
                    try {
                        face_vertices.push_back(std::stoi(vertex_index) - 1);
                    } catch (const std::exception& e) {
                        std::cerr << "Error parsing face index: " << vertex_index << std::endl;
                    }
                }
            }
            
            if (face_vertices.size() >= 3) {
                for (size_t i = 0; i < face_vertices.size() - 2; ++i) {
                    if (face_vertices[0] < vertices.size() && 
                        face_vertices[i+1] < vertices.size() && 
                        face_vertices[i+2] < vertices.size()) {
                        
                        Vector3f v0 = (vertices[face_vertices[0]] - center) * scale;
                        Vector3f v1 = (vertices[face_vertices[i+1]] - center) * scale;
                        Vector3f v2 = (vertices[face_vertices[i+2]] - center) * scale;
                        
                        mesh_add_triangle(*mesh, v0, v1, v2, material);
                        face_count++;
                    }
                }
            }
        }
    }
    
    file.close();
    
    std::cout << "Loaded " << vertices.size() << " vertices and " << face_count << " triangles" << std::endl;
    
    if (face_count == 0) {
        std::cerr << "Warning: No faces were loaded from " << filename << std::endl;
        exit(1);
    }
    
    return mesh;
}
    
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> dist(0.0f, 1.0f);

int main(int argc, char* argv[]) {
    int width = 800;
    int height = 600;
    int samples_per_pixel = 4;
    int max_depth = 5;
    
    std::string mesh_file = "";
    
    Vector3f camera_pos(0, 0, 3);
    Vector3f look_at(0, 0, 0);
    float camera_fov = 60.0f;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--file" && i + 1 < argc) {
            mesh_file = argv[i+1];
            i++;
        } else if (arg == "--width" && i + 1 < argc) {
            width = std::stoi(argv[i+1]);
            i++;
        } else if (arg == "--height" && i + 1 < argc) {
            height = std::stoi(argv[i+1]);
            i++;
        } else if (arg == "--samples" && i + 1 < argc) {
            samples_per_pixel = std::stoi(argv[i+1]);
            i++;
        } else if (arg == "--cam-z" && i + 1 < argc) {
            camera_pos.z = std::stof(argv[i+1]);
            i++;
        } else if (arg == "--cam-x" && i + 1 < argc) {
            camera_pos.x = std::stof(argv[i+1]);
            i++;
        } else if (arg == "--cam-y" && i + 1 < argc) {
            camera_pos.y = std::stof(argv[i+1]);
            i++;
        } else if (arg == "--look-x" && i + 1 < argc) {
            look_at.x = std::stof(argv[i+1]);
            i++;
        } else if (arg == "--look-y" && i + 1 < argc) {
            look_at.y = std::stof(argv[i+1]);
            i++;
        } else if (arg == "--look-z" && i + 1 < argc) {
            look_at.z = std::stof(argv[i+1]);
            i++;
        } else if (arg == "--fov" && i + 1 < argc) {
            camera_fov = std::stof(argv[i+1]);
            i++;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --file FILE     Path to OBJ file to render" << std::endl;
            std::cout << "  --width WIDTH   Image width in pixels (default: 800)" << std::endl;
            std::cout << "  --height HEIGHT Image height in pixels (default: 600)" << std::endl;
            std::cout << "  --samples N     Samples per pixel for anti-aliasing (default: 4)" << std::endl;
            std::cout << "  --cam-x X       Camera X position (default: 0)" << std::endl;
            std::cout << "  --cam-y Y       Camera Y position (default: 0)" << std::endl;
            std::cout << "  --cam-z Z       Camera Z position (default: 3)" << std::endl;
            std::cout << "  --look-x X      Look-at point X coordinate (default: 0)" << std::endl;
            std::cout << "  --look-y Y      Look-at point Y coordinate (default: 0)" << std::endl;
            std::cout << "  --look-z Z      Look-at point Z coordinate (default: 0)" << std::endl;
            std::cout << "  --fov FOV       Field of view in degrees (default: 60)" << std::endl;
            std::cout << "  --with-scene    Include the default test scene objects" << std::endl;
            std::cout << "  --help          Display this help message" << std::endl;
            return 0;
        }
    }
    
    Camera camera = camera_create(
        camera_pos,     
        look_at,       
        Vector3f(0, 1, 0),  
        camera_fov,      
        static_cast<float>(width) / height 
    );
    
    Scene scene = scene_create();
    
    scene_add_light(scene, Vector3f(5, 5, 5));
    
    
    Material mesh_material = material_create_custom(Vector3f(0.8f, 0.6f, 0.2f), 0.1f, 0.7f, 0.3f, 16.0f, 0.3f);
    std::shared_ptr<Mesh> mesh;
    
    if (!mesh_file.empty()) {
        printf("Loading mesh from file: %s\n", mesh_file.c_str());
        if (mesh_file.substr(mesh_file.find_last_of(".") + 1) == "obj") {
            mesh = load_obj_file(mesh_file, mesh_material);
        } else {
            std::cout << "Unsupported file format. Using default cube." << std::endl;
            exit(1);
        }
    } else {
        exit(1);
    }
    
    scene_add_mesh(scene, *mesh);
    
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    
    std::vector<Vector3f> pixels(width * height);
    
    std::cout << "Rendering..." << std::endl;
    std::cout << "Camera position: (" << camera_pos.x << ", " << camera_pos.y << ", " << camera_pos.z << ")" << std::endl;
    std::cout << "Looking at: (" << look_at.x << ", " << look_at.y << ", " << look_at.z << ")" << std::endl;
    
    for (int j = 0; j < height; ++j) {
        std::cout << "\rScanlines remaining: " << (height - j) << ' ' << std::flush;
        for (int i = 0; i < width; ++i) {
            Vector3f color(0, 0, 0);
            
            for (int s = 0; s < samples_per_pixel; ++s) {
                float u = (i + dist(gen)) / (width - 1);
                float v = (height - 1 - j + dist(gen)) / (height - 1);  // Flip y-axis
                
                Ray ray = camera_get_ray(camera, u, v);
                color += ray_color(ray, scene, max_depth);
            }
            
            color = color * (1.0f / samples_per_pixel);
            pixels[j * width + i] = color;
        }
    }
    
    std::cout << "\nDone rendering." << std::endl;
    
    std::string output_filename = "output_" + timestamp.str() + ".ppm";
        std::cout << "Saving image as " << output_filename << std::endl;
        save_ppm(pixels, width, height, output_filename);
        std::cout << "Image saved!" << std::endl;
    
    return 0;
}