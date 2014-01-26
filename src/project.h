#ifndef GENESIS_PROJECT_H
#define GENESIS_PROJECT_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

struct GenesisProject;

struct GenesisProject *genesis_project_new(void);
void genesis_project_close(struct GenesisProject *project);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // GENESIS_PROJECT_H
