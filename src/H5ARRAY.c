#include "H5LT.h"

#include "H5Zlzo.h"
#include "H5Zucl.h"

#include <string.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------
 * 
 * Public functions
 * 
 *-------------------------------------------------------------------------
 */


/*-------------------------------------------------------------------------
 * Function: H5ARRAYmake
 *
 * Purpose: Creates and writes a dataset of a type type_id
 *
 * Return: Success: 0, Failure: -1
 *
 * Programmer: Pedro Vicente, pvn@ncsa.uiuc.edu
 *
 * Date: March 19, 2001
 *
 * Comments: Modified by F. Alted. October 21, 2002
 *
 * Modifications: This is the same routine as H5LTmake_dataset, but I've 
 *                added a TITLE attribute for array, as well as 
 *                CLASS and VERSION attributes.
 *
 *
 *-------------------------------------------------------------------------
 */

herr_t H5ARRAYmake( hid_t loc_id, 
		    const char *dset_name,
		    const char *title,
		    const char *flavor,
		    const char *obversion,    /* The Array VERSION number */
		    int atomictype,
		    const int rank, 
		    const hsize_t *dims,
		    hid_t type_id,
		    hsize_t chunk_size,	/* New */
		    void  *fill_data,	/* New */
		    int   compress,	/* New */
		    char  *complib,	/* New */
		    int   shuffle,	/* New */
		    const void *data)
{

 hid_t   dataset_id, space_id, datatype;  
 hsize_t dataset_dims[1] = {1};	/* Only one array initially */
 hsize_t *maxdims = NULL;
 hid_t   plist_id;
 hsize_t *dims_chunk;
 unsigned int cd_values[2];
 int     enlargeable = 0;
 int     i, fixedmatsize, extdim;

 extdim = -1;   		/* Default is not extensible */
 /* Check for a dimension equal to 0 */
 for(i=0;i<rank;i++) {
   if (dims[i] == 0) {
     atomictype = 1;
     if (chunk_size == 0) {
       chunk_size = 1024;  	/* A safe default for small arrays */
     }
     extdim = i;
     break;
   }
 }
 /* If a chunk size greater than zero, the array should be enlargeable */
 if (chunk_size > 0) {
   enlargeable = 1;
 }

 fixedmatsize = 1;
 if (enlargeable) {
   maxdims = malloc(rank*sizeof(hsize_t));
   dims_chunk = malloc(rank*sizeof(hsize_t));

   /* Size for the matrices that will be added */
   for(i=0;i<rank;i++)
     if (i != extdim)
       fixedmatsize *= dims[i];

   /* Chunk size for the unlimited dimension */
   for(i=0;i<rank;i++) {
     if (i == extdim) {
       maxdims[i] = H5S_UNLIMITED;
       if (chunk_size / fixedmatsize > 0)
	 dims_chunk[i] = chunk_size / fixedmatsize;
       else
	 dims_chunk[i] = 1;
     }
     else {
       maxdims[i] = dims[i];
       dims_chunk[i] = dims[i];
     }
   }
 }

 /* Create the data space for the dataset. */
 if (atomictype) {
   if ( (space_id = H5Screate_simple( rank, dims, maxdims )) < 0 )
     return -1;
   /*
    * Define atomic datatype for the data in the file.
    */
   datatype = type_id;
 }
 else { 
   if ( (space_id = H5Screate_simple( 1, dataset_dims, maxdims )) < 0)
     return -1;
   /*
    * Define array datatype for the data in the file.
    */
   datatype = H5Tarray_create(type_id, rank, dims, NULL); 
 }   

 if (enlargeable) {
   /* Modify dataset creation properties, i.e. enable chunking  */
   plist_id = H5Pcreate (H5P_DATASET_CREATE);
   if ( H5Pset_chunk ( plist_id, rank, dims_chunk ) < 0 )
     return -1;

   /* Set the fill value using a struct as the data type. */
   if ( fill_data)
     {
       if ( H5Pset_fill_value( plist_id, datatype, fill_data ) < 0 ) 
	 return -1;
     }

   /* 
      Dataset creation property list is modified to use 
      GZIP compression with the compression effort set to 6. 
      Note that compression can be used only when dataset is chunked. 
   */

   if (shuffle) {
     if ( H5Pset_shuffle( plist_id) < 0 )
       return -1;
   }

   if (compress) {
     /* The default compressor in HDF5 (zlib) */
     if (strcmp(complib, "zlib") == 0) {
       /* Modified this to use the compress level stated in compress */
       /*   if ( H5Pset_deflate( plist_id, 6) < 0 ) */
       if ( H5Pset_deflate( plist_id, compress) < 0 )
	 return -1;
     }
     /* The LZO compressor does accept parameters */
     else if (strcmp(complib, "lzo") == 0) {
       cd_values[0] = compress;
       cd_values[1] = (int)(atof(obversion) * 10);
       if ( H5Pset_filter( plist_id, FILTER_LZO, 0, 2, cd_values) < 0 )
	 return -1;
     }
     /* The UCL compress does accept parameters */
     else if (strcmp(complib, "ucl") == 0) {
       cd_values[0] = compress;
       cd_values[1] = (int)(atof(obversion) * 10);
       if ( H5Pset_filter( plist_id, FILTER_UCL, 0, 2, cd_values) < 0 )
	 return -1;
     }
     else {
       /* Compression library not supported */
       fprintf(stderr, "Compression library not supported\n");
       return -1;
     }
   }

   /* Create the dataset. */
   if ((dataset_id = H5Dcreate(loc_id, dset_name, datatype,
			       space_id, plist_id )) < 0 )
     goto out;
 }  
 else {
   /* Create the dataset. */
   if ((dataset_id = H5Dcreate(loc_id, dset_name, datatype,
			       space_id, H5P_DEFAULT )) < 0 )
     goto out;
 }

 /* Write the dataset only if there is data to write */

 if (data) 
 {
   if ( H5Dwrite( dataset_id, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, data ) < 0 )
   goto out;
 }

 /* End access to the dataset and release resources used by it. */
 if ( H5Dclose( dataset_id ) < 0 )
  return -1;

 /* Terminate access to the data space. */ 
 if ( H5Sclose( space_id ) < 0 )
  return -1;

 /* Release the datatype in the case that it is not an atomic type */
 if (!atomictype) {
   if ( H5Tclose( datatype ) < 0 )
     return -1;
 }


/*-------------------------------------------------------------------------
 * Set the conforming array attributes
 *-------------------------------------------------------------------------
 */
    
 /* Attach the CLASS attribute */
 if ( H5LTset_attribute_string( loc_id, dset_name, "CLASS", "ARRAY" ) < 0 )
  goto out;
   
 /* Attach the CLASS attribute */
 if ( H5LTset_attribute_string( loc_id, dset_name, "FLAVOR", flavor ) < 0 )
  goto out;
   
 /* Attach the VERSION attribute */
 if ( H5LTset_attribute_string( loc_id, dset_name, "VERSION", obversion ) < 0 )
  goto out;
     
 /* Attach the TITLE attribute */
 if ( H5LTset_attribute_string( loc_id, dset_name, "TITLE", title ) < 0 )
  goto out;

 /* Attach the EXTDIM attribute */
 if ( H5LTset_attribute_int( loc_id, dset_name, "EXTDIM", &extdim, 1 ) < 0 )
  goto out;

 /* Release resources */
 free(maxdims);
 free(dims_chunk);

 return 0;

out:
 H5Dclose( dataset_id );
 H5Sclose( space_id ); 
 free(maxdims);
 free(dims_chunk);
 return -1;

}


/*-------------------------------------------------------------------------
 * Function: H5ARAYappend_records
 *
 * Purpose: Appends records to an array
 *
 * Return: Success: 0, Failure: -1
 *
 * Programmers: 
 *  Francesc Alted
 *
 * Date: October 30, 2003
 *
 * Comments: Uses memory offsets
 *
 * Modifications: 
 *
 *
 *-------------------------------------------------------------------------
 */


herr_t H5ARRAYappend_records( hid_t loc_id, 
			      const char *dset_name,
			      const int rank,
			      hsize_t *dims_orig,
			      hsize_t *dims_new,
			      const void *data )  
{

 hid_t    dataset_id;
 hid_t    type_id;
 hid_t    space_id;
 hid_t    mem_space_id;
 hsize_t  *dims, *start;
 int      i, extdim;

 /* Open the dataset. */
 if ( (dataset_id = H5Dopen( loc_id, dset_name )) < 0 )
  goto out;

 /* Get the datatype */
 if ( (type_id = H5Dget_type( dataset_id )) < 0 )
  goto out;

 /* Get the extensible dimension */
 if ( H5LTget_attribute_int( loc_id, dset_name, "EXTDIM", &extdim ) < 0 )
   goto out;

 /* Compute the arrays for new dimensions and coordinates and extents */
 dims = malloc(rank*sizeof(hsize_t));
 start = malloc(rank*sizeof(hsize_t));
 for(i=0;i<rank;i++) {
   dims[i] = dims_orig[i];
   start[i] = 0;
 }
 dims[extdim] += dims_new[extdim];
 start[extdim] = dims_orig[extdim];

 /* Extend the dataset */
 if ( H5Dextend ( dataset_id, dims ) < 0 )
  goto out;

 /* Create a simple memory data space */
 if ( (mem_space_id = H5Screate_simple( rank, dims_new, NULL )) < 0 )
  return -1;

 /* Get the file data space */
 if ( (space_id = H5Dget_space( dataset_id )) < 0 )
  return -1;

 /* Define a hyperslab in the dataset */
 if ( H5Sselect_hyperslab( space_id, H5S_SELECT_SET, start, NULL, dims_new, NULL) < 0 )
   goto out;

 if ( H5Dwrite( dataset_id, type_id, mem_space_id, space_id, H5P_DEFAULT, data ) < 0 )
     goto out;

 /* Update the original dimensions of the array after a successful append */
 dims_orig[extdim] += dims_new[extdim];

 if ( H5Sclose( space_id ) < 0 )
  goto out;
 
 /* Release the datatype. */
 if ( H5Tclose( type_id ) < 0 )
  return -1;

 /* End access to the dataset */
 if ( H5Dclose( dataset_id ) < 0 )
  goto out;

 /* Release resources */
 free(start);
 free(dims);

return 0;

out:
 H5Dclose( dataset_id );
 return -1;

}


/*-------------------------------------------------------------------------
 * Function: H5ARRAYread
 *
 * Purpose: Reads an array from disk.
 *
 * Return: Success: 0, Failure: -1
 *
 * Programmer: Francesc Alted, falted@openlc.org
 *
 * Date: October 22, 2002
 *
 *-------------------------------------------------------------------------
 */

herr_t H5ARRAYread( hid_t loc_id, 
		    const char *dset_name,
		    void *data )
{
 hid_t   dataset_id;  
 hid_t   type_id;

 /* Open the dataset. */
 if ( (dataset_id = H5Dopen( loc_id, dset_name )) < 0 )
  return -1;
 
 /* Get the datatype */
 if ( (type_id = H5Dget_type(dataset_id)) < 0 )
     return -1;
 
 /* Read */
 if (H5Dread(dataset_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0)
  goto out;

 /* End access to the dataset and release resources used by it. */
 if ( H5Dclose( dataset_id ) )
  return -1;

 /* Release resources */
 if ( H5Tclose(type_id))
   return -1;

 return 0;

out:
 H5Dclose( dataset_id );
 return -1;

}
  

/*-------------------------------------------------------------------------
 * Function: H5ARRAYget_ndims
 *
 * Purpose: Gets the dimensionality of an array.
 *
 * Return: Success: 0, Failure: -1
 *
 * Programmer: Francesc Alted
 *
 * Date: October 22, 2002
 *
 *-------------------------------------------------------------------------
 */
/* Addition: Now, this routine can deal with both array and
   atomic datatypes. F. Alted  2003-01-29 */

herr_t H5ARRAYget_ndims( hid_t loc_id, 
			 const char *dset_name,
			 int *rank )
{
  hid_t       dataset_id;  
  hid_t       type_id; 
  hid_t       space_id; 
  H5T_class_t class_arr_id;

  /* Open the dataset. */
  if ( (dataset_id = H5Dopen( loc_id, dset_name )) < 0 )
    return -1;

  /* Get the datatype handle */
  if ( (type_id = H5Dget_type( dataset_id )) < 0 )
    goto out;

  /* Get the class. */
  class_arr_id = H5Tget_class( type_id );

  /* Check if this is an array class object*/ 
  if ( class_arr_id == H5T_ARRAY ) {

    /* Get rank */
    if ( (*rank = H5Tget_array_ndims( type_id )) < 0 )
      goto out;
  }
  else {
    /* Get the dataspace handle */
    if ( (space_id = H5Dget_space( dataset_id )) < 0 )
      goto out;

    /* Get rank */
    if ( (*rank = H5Sget_simple_extent_ndims( space_id )) < 0 )
      goto out;

    /* Terminate access to the dataspace */
    if ( H5Sclose( space_id ) < 0 )
      goto out;
 
  }


 /* Terminate access to the datatype */
 if ( H5Tclose( type_id ) < 0 )
  goto out;

 /* End access to the dataset */
 if ( H5Dclose( dataset_id ) )
  return -1;

 return 0;

out:
 H5Dclose( dataset_id );
 return -1;

}


/* Modified version of H5LTget_dataset_info present on HDF_HL
 * I had to add the capability to get the sign of
 * the array type.
 * I should request to NCSA to add this feature. */
/* Addition: Now, this routine can deal with both array and
   atomic datatypes. 2003-01-29 */

herr_t H5ARRAYget_info( hid_t loc_id, 
			const char *dset_name,
			hsize_t *dims,
			H5T_class_t *class_id,
			H5T_sign_t *sign, 
			char *byteorder,
			size_t *type_size,
			size_t *type_precision )
{
  hid_t       dataset_id;  
  hid_t       type_id;
  hid_t       space_id; 
  H5T_class_t class_arr_id;
  H5T_order_t order;
  hid_t       super_type_id; 

  /* Open the dataset. */
  if ( (dataset_id = H5Dopen( loc_id, dset_name )) < 0 )
    return -1;

  /* Get an identifier for the datatype. */
  type_id = H5Dget_type( dataset_id );

  /* Get the class. */
  class_arr_id = H5Tget_class( type_id );

  /* Check if this is an array class object*/ 
  if ( class_arr_id == H5T_ARRAY ) {

    /* Get the array base component */
    super_type_id = H5Tget_super( type_id );
 
    /* Get the class of base component. */
    *class_id = H5Tget_class( super_type_id );

    /* Get the sign in case the class is an integer. */
    if ( (*class_id == H5T_INTEGER) ) /* Only class integer can be signed */
      *sign = H5Tget_sign( type_id );
    else 
      *sign = -1;
   
    /* Get the size. */
    *type_size = H5Tget_size( super_type_id );
 
    /* Get the byteorder */
    /* Only class integer and float can be byteordered */
    if ( (*class_id == H5T_INTEGER) || (*class_id == H5T_FLOAT) ) {
      order = H5Tget_order( super_type_id );
      if (order == H5T_ORDER_LE) 
	strcpy(byteorder, "little");
      else if (order == H5T_ORDER_BE)
	strcpy(byteorder, "big");
      else {
	fprintf(stderr, "Error: unsupported byteorder: %d\n", order);
	goto out;
      }
    }
    else {
      strcpy(byteorder, "non-relevant");
    }

     /* Get dimensions */
    if ( H5Tget_array_dims(type_id, dims, NULL) < 0 )
      goto out;

    /* Release the super datatype. */
    if ( H5Tclose( super_type_id ) )
      return -1;
  }
  else {
    *class_id = class_arr_id;
    /* Get the sign in case the class is an integer. */
    if ( (*class_id == H5T_INTEGER) ) /* Only class integer can be signed */
      *sign = H5Tget_sign( type_id );
    else 
      *sign = -1;
   
    /* Get the size. */
    *type_size = H5Tget_size( type_id );
   
    /* Get the precision */
    *type_precision = H5Tget_precision( type_id );
   
    /* Get the byteorder */
    /* Only class integer and float can be byteordered */
    if ( (*class_id == H5T_INTEGER) || (*class_id == H5T_FLOAT) ) {
      order = H5Tget_order( type_id );
      if (order == H5T_ORDER_LE) 
	strcpy(byteorder, "little");
      else if (order == H5T_ORDER_BE)
	strcpy(byteorder, "big");
      else {
	fprintf(stderr, "Error: unsupported byteorder: %d\n", order);
	goto out;
      }
    }
    else {
      strcpy(byteorder, "non-relevant");
    }

    
    /* Get the dataspace handle */
    if ( (space_id = H5Dget_space( dataset_id )) < 0 )
      goto out;

    /* Get dimensions */
    if ( H5Sget_simple_extent_dims( space_id, dims, NULL) < 0 )
      goto out;

    /* Terminate access to the dataspace */
    if ( H5Sclose( space_id ) < 0 )
      goto out;
 
  }

  /* Release the datatype. */
  if ( H5Tclose( type_id ) )
    return -1;

  /* End access to the dataset */
  if ( H5Dclose( dataset_id ) )
    return -1;

  return 0;

out:
 H5Tclose( type_id );
 H5Dclose( dataset_id );
 return -1;

}



