#ifndef OFFREADER_H
#define OFFREADER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct Vt {
	float x,y,z;
	float r,g,b;
	Vector3f normal;
	int numIcidentTri;
}Vertex;

typedef struct Pgn {
	int noSides;
	int *v;
}Polygon;

typedef struct offmodel {
	Vertex *vertices;
	Polygon *polygons;
	int numberOfVertices;
 	int numberOfPolygons;
	float minX, minY, minZ, maxX, maxY, maxZ;
	float extent;
}OffModel;

OffModel* triangulateModel(OffModel* original) {
    // Count the total number of triangles needed
    int totalTriangles = 0;
    for (int i = 0; i < original->numberOfPolygons; i++) {
        if (original->polygons[i].noSides == 3) {
            totalTriangles += 1;  // Already a triangle
        } else if (original->polygons[i].noSides > 3) {
            totalTriangles += (original->polygons[i].noSides - 2);  // N-2 triangles for N-sided polygon
        }
    }
    
    // Create new model with only triangles
    OffModel* triangulated = (OffModel*)malloc(sizeof(OffModel));
    triangulated->vertices = original->vertices;  // Share vertices
    triangulated->numberOfVertices = original->numberOfVertices;
    triangulated->minX = original->minX;
    triangulated->minY = original->minY;
    triangulated->minZ = original->minZ;
    triangulated->maxX = original->maxX;
    triangulated->maxY = original->maxY;
    triangulated->maxZ = original->maxZ;
    triangulated->extent = original->extent;
    
    triangulated->numberOfPolygons = totalTriangles;
    triangulated->polygons = (Polygon*)malloc(totalTriangles * sizeof(Polygon));
    
    int triIndex = 0;
    for (int i = 0; i < original->numberOfPolygons; i++) {
        Polygon* poly = &original->polygons[i];
        
        if (poly->noSides == 3) {
            // Copy triangle directly
            triangulated->polygons[triIndex].noSides = 3;
            triangulated->polygons[triIndex].v = (int*)malloc(3 * sizeof(int));
            triangulated->polygons[triIndex].v[0] = poly->v[0];
            triangulated->polygons[triIndex].v[1] = poly->v[1];
            triangulated->polygons[triIndex].v[2] = poly->v[2];
            triIndex++;
        } 
        else if (poly->noSides > 3) {
            // Triangulate using fan approach
            for (int j = 1; j < poly->noSides - 1; j++) {
                triangulated->polygons[triIndex].noSides = 3;
                triangulated->polygons[triIndex].v = (int*)malloc(3 * sizeof(int));
                triangulated->polygons[triIndex].v[0] = poly->v[0];     // First vertex
                triangulated->polygons[triIndex].v[1] = poly->v[j];     // Current vertex
                triangulated->polygons[triIndex].v[2] = poly->v[j + 1]; // Next vertex
                triIndex++;
            }
        }
    }
    
    return triangulated;
}

OffModel* readOffFile(char * OffFile) {
    FILE * input;
    char type[4]; // Increased size to include null terminator
    int noEdges;
    int i, j;
    float x, y, z;
    int n, v;
    int nv, np;
    OffModel *model;

    input = fopen(OffFile, "r");
    if (!input) {
        printf("Error: Could not open file %s\n", OffFile);
        exit(1);
    }

    // Read and print the file type
    fscanf(input, "%3s", type); // Read up to 3 characters
    type[3] = '\0'; // Ensure null termination
    printf("\nType: %s\n", type);

    /* First line should be OFF */
    if (strcmp(type, "OFF") != 0) {
        printf("Not a OFF file\n");
        fclose(input);
        exit(1);
    }

    // Read and print the number of vertices, faces, and edges
    fscanf(input, "%d", &nv);
    fscanf(input, "%d", &np);
    fscanf(input, "%d", &noEdges);
    // printf("Number of vertices: %d, Number of polygons: %d, Number of edges: %d\n", nv, np, noEdges);

    model = (OffModel*)malloc(sizeof(OffModel));
    model->numberOfVertices = nv;
    model->numberOfPolygons = np;

    /* Allocate required data */
    model->vertices = (Vertex *) malloc(nv * sizeof(Vertex));
    model->polygons = (Polygon *) malloc(np * sizeof(Polygon));
	

    /* Read the vertices' location */
    // printf("Vertices:\n");
    for (i = 0; i < nv; i++) {
        fscanf(input, "%f %f %f", &x, &y, &z);
        model->vertices[i].x = x;
        model->vertices[i].y = y;
        model->vertices[i].z = z;
        model->vertices[i].numIcidentTri = 0;
		model->vertices[i].r = 1.0f; // Initialize color
		model->vertices[i].g = 1.0f; // Initialize color
		model->vertices[i].b = 1.0f; // Initialize color

        // printf("  Vertex %d: (%f, %f, %f)\n", i, x, y, z);

        if (i == 0) {
            model->minX = model->maxX = x;
            model->minY = model->maxY = y;
            model->minZ = model->maxZ = z;
        } else {
            if (x < model->minX) model->minX = x;
            if (x > model->maxX) model->maxX = x;
            if (y < model->minY) model->minY = y;
            if (y > model->maxY) model->maxY = y;
            if (z < model->minZ) model->minZ = z;
            if (z > model->maxZ) model->maxZ = z;
        }
    }

    /* Read the polygons */
    printf("Polygons:\n");
    for (i = 0; i < np; i++) {
        fscanf(input, "%d", &n);
        model->polygons[i].noSides = n;
        model->polygons[i].v = (int *) malloc(n * sizeof(int));
        // printf("  Polygon %d: %d sides, vertices: ", i, n);
        for (j = 0; j < n; j++) {
            fscanf(input, "%d", &v);
            model->polygons[i].v[j] = v;
            // printf("%d ", v);
        }
        // printf("\n");
    }

    float extentX = model->maxX - model->minX;
    float extentY = model->maxY - model->minY;
    float extentZ = model->maxZ - model->minZ;
    model->extent = (extentX > extentY) ? ((extentX > extentZ) ? extentX : extentZ) : ((extentY > extentZ) ? extentY : extentZ);

    // printf("Bounding Box: Min(%f, %f, %f), Max(%f, %f, %f)\n", model->minX, model->minY, model->minZ, model->maxX, model->maxY, model->maxZ);
    // printf("Extent: %f\n", model->extent);

    fclose(input);
    // return model;
    OffModel* triangulatedModel = triangulateModel(model);
    
    // Free the original model's polygon data as we don't need it anymore
    // (but keep the vertex data which is shared)
    for (int i = 0; i < model->numberOfPolygons; i++) {
        free(model->polygons[i].v);
    }
    free(model->polygons);
    free(model);
    
    return triangulatedModel;
}

// OffModel* readOffFile(char * OffFile) {
// 	FILE * input;
// 	char type[3]; 
// 	int noEdges;
// 	int i,j;
// 	float x,y,z;
// 	int n, v;
// 	int nv, np;
// 	OffModel *model;


// 	input = fopen(OffFile, "r");
// 	if (!input) {
// 		printf("Error: Could not open file %s\n", OffFile);
// 		exit(1);
// 	}
// 	fscanf(input, "%s", type);
// 	printf("\nType: %s\n", type);
// 	/* First line should be OFF */
// 	if(strncmp(type,"OFF",3)!=0) {
// 		printf("Not a OFF file");
// 		exit(1);
// 	}
// 	/* Read the no. of vertices, faces and edges */
// 	fscanf(input, "%d", &nv);
// 	fscanf(input, "%d", &np);
// 	fscanf(input, "%d", &noEdges);

// 	model = (OffModel*)malloc(sizeof(OffModel));
// 	model->numberOfVertices = nv;
// 	model->numberOfPolygons = np;
	
	
// 	/* allocate required data */
// 	model->vertices = (Vertex *) malloc(nv * sizeof(Vertex));
// 	model->polygons = (Polygon *) malloc(np * sizeof(Polygon));
	

// 	/* Read the vertices' location*/	
// 	for(i = 0;i < nv;i ++) {
// 		fscanf(input, "%f %f %f", &x,&y,&z);
// 		(model->vertices[i]).x = x;
// 		(model->vertices[i]).y = y;
// 		(model->vertices[i]).z = z;
// 		(model->vertices[i]).numIcidentTri = 0;
// 		if (i==0){
// 			model->minX = model->maxX = x; 
// 			model->minY = model->maxY = y; 
// 			model->minZ = model->maxZ = z; 
// 		} else {
// 			if (x < model->minX) model->minX = x;
// 			else if (x > model->maxX) model->maxX = x;
// 			if (y < model->minY) model->minY = y;
// 			else if (y > model->maxY) model->maxY = y;
// 			if (z < model->minZ) model->minZ = z;
// 			else if (z > model->maxZ) model->maxZ = z;
// 		}
// 	}

// 	/* Read the Polygons */	
// 	for(i = 0;i < np;i ++) {
// 		/* No. of sides of the polygon (Eg. 3 => a triangle) */
// 		fscanf(input, "%d", &n);
		
// 		(model->polygons[i]).noSides = n;
// 		(model->polygons[i]).v = (int *) malloc(n * sizeof(int));
// 		/* read the vertices that make up the polygon */
// 		for(j = 0;j < n;j ++) {
// 			fscanf(input, "%d", &v);
// 			(model->polygons[i]).v[j] = v;
// 		}
// 	}
	
// 	float extentX = model->maxX - model->minX;
// 	float extentY = model->maxY - model->minY;
// 	float extentZ = model->maxZ - model->minZ;
// 	model->extent = (extentX > extentY) ? ((extentX > extentZ) ? extentX : extentZ) : ((extentY > extentZ) ? extentY : extentZ);

// 	fclose(input);
// 	return model;
// }

int FreeOffModel(OffModel *model)
{
	int i;
	if( model == NULL )
		return 0;
	free(model->vertices);
	for( i = 0; i < model->numberOfPolygons; ++i )
	{
		if( (model->polygons[i]).v )
		{
			free((model->polygons[i]).v);
		}
	}
	free(model->polygons);
	free(model);
	return 1;
}

#endif
