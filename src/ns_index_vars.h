#ifndef NS_INDEX_VAR_H
#define NS_INDEX_VAR_H

#ifndef CAV_MAIN
extern VarTableEntry *indexVarTable;
#else
extern __thread VarTableEntry *indexVarTable;
#endif
extern PointerTableEntry_Shr* get_index_var_val(VarTableEntry_Shr* idxVarEntry, VUser *vptr, int flag, int cur_seq);
extern int input_indexvar_data(char *line, int line_number, int sess_idx, char *script_filename);
extern void copy_indexvar_into_shared_mem();

#endif  /*  NS_INDEX_VAR_H */
