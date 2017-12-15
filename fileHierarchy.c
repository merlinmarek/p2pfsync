#include "fileHierarchy.h"

#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>

#include "defines.h"
#include "md5.h"
#include "logger.h"

// helper functions
int isDirectory(const char* path) {
	struct stat info;
	// lstat does not follow symlinks
	lstat(path, &info);
	return S_ISDIR(info.st_mode);
}

// 0 = success, -1 = fail
int fileMD5(unsigned char result[16], const char* filePath) {
	// testing md5 here
	MD5_CTX md5hash;
	MD5_Init(&md5hash);

	// this assumes that the file is small enough to fit entirely in ram
	struct stat st;
	if(stat(filePath, &st) != 0) {
		// the file does not exist
		logger("file %s does not exist\n", filePath);
		return -1;
	}
	if(!S_ISREG(st.st_mode)) {
		logger("%s is not a regular file\n", filePath);
		return -1;
	}
	char* buffer = (char*)malloc(st.st_size);
	int filefd = open(filePath, O_RDONLY);
	if(filefd == -1) {
		logger("error opening file\n");
	}
	read(filefd, buffer, st.st_size);
	MD5_Update(&md5hash, (void*)buffer, st.st_size);
	MD5_Final(result, &md5hash);
	free(buffer);
	return 0;
}

FileHierarchyNode* generateHierarchy(const char* path) {
	struct stat info;
	// lstat does not follow symlinks
	if(lstat(path, &info) != 0) {
		// the file/dir does not exist
		// only if this is the directory BASE_PATH we want to create it and proceed
		if(strcmp(path, BASE_PATH) == 0) {
			// create directory and generate a new node for it
			logger("creating base path %s\n", BASE_PATH);
			if(mkdir(BASE_PATH, 777) != 0) {
				// mkdir failed
				logger("mkdir %s\n", strerror(errno));
				return NULL;
			}
			// directory created successfully
			// at this point the directory is empty so we generate new node with all zeros except the name
			FileHierarchyNode* newNode = (FileHierarchyNode*)malloc(sizeof(FileHierarchyNode));
			memset(newNode, 0, sizeof(FileHierarchyNode));
			strcpy(newNode->name, path);
			newNode->type = TYPE_DIRECTORY;
			return newNode;
		}
	}

	// lstat succeded
	// check if we have a directory here or a file
	if(S_ISREG(info.st_mode)) {
		// this is a file (leaf node)
		// we can just generate a new node and append it to the hierarchy

		// first try to open and generate the md5
		// if this fails we do not have to create the node
		unsigned char md5hash[16];
		memset(md5hash, 0, 16);
		if(fileMD5(md5hash, path) != 0) {
			return NULL;
		}

		// now we have an md5sum and the file exists etc. we can generate the node now
		FileHierarchyNode* newNode = (FileHierarchyNode*)malloc(sizeof(FileHierarchyNode));
		memset(newNode, 0, sizeof(FileHierarchyNode));
		newNode->type = TYPE_FILE;

		// to use basename/dirname we first have to copy the string, because basename will operate on the string
		char* pathCopy = malloc(strlen(path) + 1);
		strcpy(pathCopy, path);
		const char* fileName = basename(pathCopy);
		strcpy(newNode->name, fileName);
		memcpy(newNode->md5Hash, md5hash, 16);
		free(pathCopy); // only do this after we are done with basename!!!
		return newNode;
	} else if(S_ISDIR(info.st_mode)) {
		// this is a directory
		//FileHierarchyNode* newNode = (FileHierarchyNode*)malloc(sizeof(FileHierarchyNode));
		// create a node for this directory

		// I NEED TO CONINUE HERE AKDSJFLJKASGHJKASHDFOASDHF
		// ASÖKLDJGÖLHGÖKASHFDKÖASHDFAÖKSDFHSDÖKLFJHASDF
		// ASÖKLDJGÖLHGÖKASHFDKÖASHDFAÖKSDFHSDÖKLFJHASDF
		// ASÖKLDJGÖLHGÖKASHFDKÖASHDFAÖKSDFHSDÖKLFJHASDF
		// ASÖKLDJGÖLHGÖKASHFDKÖASHDFAÖKSDFHSDÖKLFJHASDF
		// ASÖKLDJGÖLHGÖKASHFDKÖASHDFAÖKSDFHSDÖKLFJHASDF
		// ASÖKLDJGÖLHGÖKASHFDKÖASHDFAÖKSDFHSDÖKLFJHASDF
		// ASÖKLDJGÖLHGÖKASHFDKÖASHDFAÖKSDFHSDÖKLFJHASDF
		// ASÖKLDJGÖLHGÖKASHFDKÖASHDFAÖKSDFHSDÖKLFJHASDF
		// ASÖKLDJGÖLHGÖKASHFDKÖASHDFAÖKSDFHSDÖKLFJHASDF
		// ASÖKLDJGÖLHGÖKASHFDKÖASHDFAÖKSDFHSDÖKLFJHASDF

		// list all contents in the directory!
		// call this function recursively on all of them to obtain nodes
		// set parent to this node
		// get md5 for this directory by all children
	} else {
		// some file/dir type we do not want to handle
		logger("skipping non file/dir %s\n", path);
	}

	logger("%s is %s directory\n", path, isDirectory(path) ? "" : "not");
	return NULL;
}

// printing should also be done recursively
void printHierarchy(FileHierarchyNode* root) {

}
