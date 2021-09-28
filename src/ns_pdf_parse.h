#ifndef __NS_PDF_PARSE_H__
#define __NS_PDF_PARSE_H__

//void insert_into_unique_pdf_list(int pdf_id);
FILE *get_and_open_pdf_file_by_pdf_id(int pdf_id, char *pdf_name);
int parse_pdf(FILE *pdf_fd, int *min_granules, int *max_granules);
//int process_pdf(int pdf_id, int *min_granules, int *max_granules);
int process_pdf(int *pdf_id, int *min_granules, int *max_granules, char *pdf_name);
FILE *get_pdf_fd_id_from_pdf(char * pdf_file, int *pdf_id);

#define GET_FILE_NAME_FORMAT "grep -H PDF %s/pdf/*.pdf | grep -v \":#\" | awk -F'|' '{if ($2 == %d) print $1}' | cut -d':' -f1"

#endif /* __NS_PDF_PARSE_H__ */
