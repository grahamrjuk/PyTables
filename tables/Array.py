########################################################################
#
#       License: BSD
#       Created: October 10, 2002
#       Author:  Francesc Alted - falted@openlc.org
#
#       $Source: /home/ivan/_/programari/pytables/svn/cvs/pytables/pytables/tables/Array.py,v $
#       $Id: Array.py,v 1.35 2003/11/04 13:41:53 falted Exp $
#
########################################################################

"""Here is defined the Array class.

See Array class docstring for more info.

Classes:

    Array

Functions:


Misc variables:

    __version__


"""

__version__ = "$Revision: 1.35 $"

# default version for ARRAY objects
#obversion = "1.0"    # initial version
obversion = "2.0"    # support of enlargeable arrays


import types, warnings, sys
from Leaf import Leaf
from utils import calcBufferSize
import hdf5Extension
import numarray
import numarray.strings as strings

try:
    import Numeric
    Numeric_imported = 1
except:
    Numeric_imported = 0

class Array(Leaf, hdf5Extension.Array, object):
    """Represent an homogeneous dataset in HDF5 file.

    It enables to create new datasets on-disk from Numeric, numarray,
    lists, tuples, strings or scalars, or open existing ones.

    All Numeric and numarray typecodes are supported except for complex
    datatypes.

    Methods:

      Common to all leaves:
        close()
        flush()
        getAttr(attrname)
        rename(newname)
        remove()
        setAttr(attrname, attrvalue)
        
      Specific of Array:
        read()

    Instance variables:

      Common to all leaves:
        name -- the leaf node name
        hdf5name -- the HDF5 leaf node name
        title -- the leaf title
        shape -- the leaf shape
        byteorder -- the byteorder of the leaf
        
      Specific of Array:
        type -- the type class for the array
        flavor -- the object type of this object (Numarray, Numeric, List,
                  Tuple, String, Int of Float)
        enlargeable -- tells if the Array can grow or not

    """
    
    def __init__(self, object = None, title = "", atomictype = 1,
                 enlargeable = 0, compress = 0, complib = "zlib",
                 shuffle = 0, expectedobjects = 1000):
        """Create the instance Array.

        Keyword arguments:

        object -- Regular object to be saved. It can be any of
                  Numarray, Numeric, List, Tuple, String, Int of Float
                  types, provided that they are regular (i.e. they are
                  not like [[1,2],2]). If None, the metadata for the
                  array will be taken from disk.

        title -- Sets a TITLE attribute on the HDF5 array entity.
        
        atomictype -- is a boolean that specifies the underlying HDF5
            type; if 1 an atomic data type (i.e. it can't be
            decomposed in smaller types) is used; if 0 an HDF5 array
            datatype is used. Note: using an atomic type is not
            compatible with an enlargeable Array (see above).

        enlargeable -- a boolean specifying whether the Array object
            could be enlarged or not by appending more elements like
            "object" ones.

        compress -- Specifies a compress level for data. The allowed
            range is 0-9. A value of 0 disables compression and this
            is the default. A value greater than 0 implies enlargeable
            Arrays (see above).

        complib -- Specifies the compression library to be used. Right
            now, "zlib", "lzo" and "ucl" values are supported.

        shuffle -- Whether or not to use the shuffle filter in HDF5. This
            is normally used to improve the compression ratio.

        expectedobjects -- In the case of enlargeable arrays this
            represents an user estimate about the number of object
            elements that will be added to the Array object. If not
            provided, the default value is 1000 objects. If you plan
            to create both much smaller or much bigger Arrays try
            providing a guess; this will optimize the HDF5 B-Tree
            creation and management process time and the amount of
            memory used.

        """
        self.new_title = title
        self._v_atomictype = atomictype
        self.enlargeable = enlargeable
        self._v_compress = compress
        self._v_complib = complib
        self._v_shuffle = shuffle
        self._v_expectedobjects = expectedobjects
        # Check if we have to create a new object or read their contents
        # from disk
        if object is not None:
            self._v_new = 1
            self.object = object
        else:
            self._v_new = 0

    def _convertIntoNA(self, object):
        "Convert a generic object into a numarray object"
        arr = object
        # Check for Numeric objects
        if isinstance(arr, numarray.NumArray):
            flavor = "NumArray"
            naarr = arr
            byteorder = arr._byteorder
        elif (Numeric_imported and type(arr) == type(Numeric.array(1))):
            flavor = "Numeric"
            if arr.typecode() == "c":
                # To emulate as close as possible Numeric character arrays,
                # itemsize for chararrays will be always 1
                if arr.iscontiguous():
                    # This the fastest way to convert from Numeric to numarray
                    # because no data copy is involved
                    naarr = strings.array(buffer(arr),
                                          itemsize=1,
                                          shape=arr.shape)
                else:
                    # Here we absolutely need a copy so as to obtain a buffer.
                    # Perhaps this can be avoided or optimized by using
                    # the tolist() method, but this should be tested.
                    naarr = strings.array(buffer(arr.copy()),
                                          itemsize=1,
                                          shape=arr.shape)
            else:
                if arr.iscontiguous():
                    # This the fastest way to convert from Numeric to numarray
                    # because no data copy is involved
                    naarr = numarray.array(buffer(arr),
                                           type=arr.typecode(),
                                           shape=arr.shape)
                else:
                    # Here we absolutely need a copy in order
                    # to obtain a buffer.
                    # Perhaps this can be avoided or optimized by using
                    # the tolist() method, but this should be tested.
                    naarr = numarray.array(buffer(arr.copy()),
                                           type=arr.typecode(),
                                           shape=arr.shape)                    

        elif (isinstance(arr, strings.CharArray)):
            flavor = "CharArray"
            naarr = arr
        elif (isinstance(arr, types.TupleType) or
              isinstance(arr, types.ListType)):
            # Test if can convert to numarray object
            try:
                naarr = numarray.array(arr)
            # If not, test with a chararray
            except TypeError:
                try:
                    naarr = strings.array(arr)
                # If still doesn't, issues an error
                except:
                    raise ValueError, \
"""The object '%s' can't be converted to a numerical or character array.
  Sorry, but this object is not supported.""" % (arr)
            if isinstance(arr, types.TupleType):
                flavor = "Tuple"
            else:
                flavor = "List"
        elif isinstance(arr, types.IntType):
            naarr = numarray.array(arr)
            flavor = "Int"
        elif isinstance(arr, types.FloatType):
            naarr = numarray.array(arr)

            flavor = "Float"
        elif isinstance(arr, types.StringType):
            naarr = strings.array(arr)
            flavor = "String"
        else:
            raise ValueError, \
"""The object '%s' is not in the list of supported objects (numarray,
  chararray,homogeneous list or homogeneous tuple, int, float or str).
  Sorry, but this object is not supported.""" % (arr)

        # We always want a contiguous buffer
        # (no matter if has an offset or not; that will be corrected later)
        if (not naarr.iscontiguous()):
            # Do a copy of the array in case is not contiguous
            naarr = numarray.NDArray.copy(naarr)

        return (naarr, flavor)
            
    def _create(self):
        """Save a fresh array (i.e., not present on HDF5 file)."""
        global obversion

        self._v_version = obversion
        naarr, flavor = self._convertIntoNA(self.object)

        if (isinstance(naarr, strings.CharArray)):
            self.byteorder = "non-relevant" 
        else:
            self.byteorder  = naarr._byteorder

        # Check for null dimensions
        zerodims = numarray.sum(numarray.array(naarr.shape) == 0)
        if zerodims > 0:
            if zerodims == 1:
                # If there is some zero dimension, set the Array as
                # enlargeable
                self.enlargeable = 1
            else:
                raise NotImplementedError, "Multiple zero-dimension on arrays is not supported"

        # Compute some values for buffering and I/O parameters
        if self.enlargeable:
            # Compute the rowsize for each element
            self.rowsize = naarr._type.bytes
            for i in naarr.shape:
                if i>0:
                    self.rowsize *= i
            # Compute the optimal chunksize
            (self._v_maxTuples, self._v_chunksize) = \
               calcBufferSize(self.rowsize, self._v_expectedobjects,
                              self._v_compress)
        else:
            (self._v_maxTuples, self._v_chunksize) = (1,0)

        self.shape = naarr.shape
        self.itemsize = naarr._itemsize
        self.flavor = flavor
        self.type = self._createArray(naarr, self.new_title)

    def _open(self):
        """Get the metadata info for an array in file."""
        (self.type, self.shape, self.itemsize, self.byteorder) = \
                        self._openArray()

    def append(self, object):
        """Append the object to this enlargeable object"""

        # Convert the object to a numarray object
        naarr, flavor = self._convertIntoNA(object)
        # If you don't want to mix different flavors, uncomment this
#         if flavor <> self.flavor:
#             raise RuntimeError, \
# """You are trying to append an object with flavor '%s' to an Array object with flavor '%s'. Please, try to supply objects with the same flavor.""" % \
#             (flavor, self.flavor)
        
        # Add conversion procedures as well as checks for
        # conforming objects to be added
        # First, self is extensible?
        extdim = self.attrs.EXTDIM
        assert extdim <> None, "Sorry, the Array %s is not enlargeable." % \
               (self.pathname)
        # Next, the arrays conforms self expandibility?
        assert len(self.shape) == len(naarr.shape), \
"""Sorry, the ranks of the Array '%s' and object to be appended differ
  (%d <> %d).""" % (self._v_pathname, len(self.shape), len(naarr.shape))
        for i in range(len(self.shape)):
            if i <> extdim:
                assert self.shape[i] == naarr.shape[i], \
"""Sorry, shapes of Array '%s' and object differ in dimension %d (non-enlargeable)""" % (self._v_pathname, i) 
        
        self._append(naarr)

    # Accessor for the _readArray method in superclass
    def read(self):
        """Read the array from disk and return it as numarray."""

        if repr(self.type) == "CharType":
            arr = strings.array(None, itemsize=self.itemsize,
                                  shape=self.shape)
        else:
            arr = numarray.array(buffer=None,
                                 type=self.type,
                                 shape=self.shape)
            # Set the same byteorder than on-disk
            arr._byteorder = self.byteorder

        # Protection against empty arrays on disk
        zerodim = 0
        for i in range(len(self.shape)):
            if self.shape[i] == 0:
                zerodim = 1
                
        if not zerodim:
            # Arrays that have not zero dimensionality
            self._readArray(arr._data)

        # Numeric, NumArray, CharArray, Tuple, List, String, Int or Float
        self.flavor = self.getAttr("FLAVOR")
        
        # Convert to Numeric, tuple or list if needed
        if self.flavor == "Numeric":
            if Numeric_imported:
                # This works for both numeric and chararrays
                # arr=Numeric.array(arr, typecode=arr.typecode())
                # The next is 10 times faster (for tolist(),
                # we should check for tostring()!)
                if repr(self.type) == "CharType":
                    arrstr = arr.tostring()
                    arr=Numeric.reshape(Numeric.array(arrstr), arr.shape)
                else:
                    # tolist() method creates a list with a sane byteorder
                    if arr.shape <> ():
                        arr=Numeric.array(arr.tolist(), typecode=arr.typecode())
                    else:
                        # This works for rank-0 arrays
                        # (but is slower for big arrays)
                        arr=Numeric.array(arr, typecode=arr.typecode())
                        
            else:
                # Warn the user
                warnings.warn( \
"""The object on-disk is type Numeric, but Numeric is not installed locally.
  Returning a numarray object instead!.""")
        elif self.flavor == "Tuple":
            arr = tuple(arr.tolist())
        elif self.flavor == "List":
            arr = arr.tolist()
        elif self.flavor == "Int":
            arr = int(arr)
        elif self.flavor == "Float":
            arr = float(arr)
        elif self.flavor == "String":
            arr = arr.tostring()
        
        return arr
        
    # Moved out of scope
    def _g_del__(self):
        """Delete some objects"""
        print "Deleting Array object"
        pass

    def __repr__(self):
        """This provides more metainfo in addition to standard __str__"""

        return "%s\n  type = %r\n  itemsize = %r\n  flavor = %r\n  byteorder = %r" % \
               (self, self.type, self.itemsize, self.attrs.FLAVOR, self.byteorder)
