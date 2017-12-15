#ifndef FILE_HIERARCHY_H
#define FILE_HIERARCHY_H

#define TYPE_FILE 5
#define TYPE_DIRECTORY 6

typedef struct fileHierarchyNode {
	struct fileHierarchyNode** childrenList;
	struct fileHierarchyNode* parent;
	unsigned char type; // either TYPE_FILE or TYPE_DIRECTORY

	// for files this is the md5hash of its contents, for empty directories its all zeros
	// for non-empty directories it is the md5 hash of all child hashes concatenated
	unsigned char md5Hash[16];

	// name length limits are 255 bytes on most file systems
	// this is only the actual name
	// to generate a path out of this we need to traverse the parents as well
	char name[256];
} FileHierarchyNode;


// recursively generates the Hierarchy for a given path
// so generateHierarchy("./sync_files") generates a hierarchy with
// sync_files as its root
// THIS FUNCTION IS PROBABLY very time consuming. It will calculate the md5
// hashes of all files in all subdirectories!! Later there should probably be
// a way to cache this over sessions
FileHierarchyNode* generateHierarchy(const char* basePath);

// printing should also be done recursively
void printHierarchy(FileHierarchyNode* root);

#endif
