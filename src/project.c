#include "project.h"
#include <stdlib.h>

struct GenesisProject {
    int foo;
};



struct GenesisProject *genesis_project_new(void)
{
    struct GenesisProject *project = calloc(1, sizeof(struct GenesisProject));

    return project;
}


void genesis_project_close(struct GenesisProject *project)
{
    free(project);
}
