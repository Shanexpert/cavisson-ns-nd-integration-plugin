/*  
  Name   : sortableTable.js
  Author : Prateek/Neeraj
  Purpose: Provides functionality for table sorting.
           <Fill details here>
  Notes  :
    src-->http://webfx.eae.net/dhtml/sortabletable/sortabletable.html

  Modification History:
    05/19/06:Abhishek:1.4 - add function to sort IP values for table
    05/26/06:Abhishek:1.4 - fix bug of date sorting function 
	06/01/06:Abhishek:1.4 - add function to sort Number with '-'
*/

function SortableTable(oTable, oSortTypes) {
	this.sortTypes = oSortTypes || [];
	this.sortColumn = null;
	this.descending = null;

	var oThis = this;
	this._headerOnclick = function (e)
	{
      oThis.headerOnclick(e);
	};
  
	if (oTable) 
	{
	  this.setTable( oTable );
	  this.document = oTable.ownerDocument || oTable.document;
	}
	else
	{
	  this.document = document;
	}


	// only IE needs this
	var win = this.document.defaultView || this.document.parentWindow;
  //alert(win);
	this._onunload = function () {
		oThis.destroy();
	};
	if (win && typeof win.attachEvent != "undefined") {
		win.attachEvent("onunload", this._onunload);
	}
}

SortableTable.gecko = navigator.product == "Gecko";
SortableTable.msie = /msie/i.test(navigator.userAgent);
// Mozilla is faster when doing the DOM manipulations on
// an orphaned element. MSIE is not
SortableTable.removeBeforeSort = SortableTable.gecko;

SortableTable.prototype.onsort = function () {};

// default sort order. true -> descending, false -> ascending
SortableTable.prototype.defaultDescending = false;

// shared between all instances. This is intentional to allow external files
// to modify the prototype
SortableTable.prototype._sortTypeInfo = {};

SortableTable.prototype.setTable = function (oTable) {
	if ( this.tHead )
		this.uninitHeader();
	this.element = oTable;
	this.setTHead( oTable.tHead );
	this.setTBody( oTable.tBodies[0] );
};

SortableTable.prototype.setTHead = function (oTHead) {
	if (this.tHead && this.tHead != oTHead )
		this.uninitHeader();
	this.tHead = oTHead;
	this.initHeader( this.sortTypes );
};

SortableTable.prototype.setTBody = function (oTBody) {
	this.tBody = oTBody;
};

SortableTable.prototype.setSortTypes = function ( oSortTypes ) {
	if ( this.tHead )
		this.uninitHeader();
	this.sortTypes = oSortTypes || [];
	if ( this.tHead )
		this.initHeader( this.sortTypes );
};

// adds arrow containers and events
// also binds sort type to the header cells so that reordering columns does
// not break the sort types
SortableTable.prototype.initHeader = function (oSortTypes) {
	if (!this.tHead) return;
	var cells = this.tHead.rows[0].cells;
	var doc = this.tHead.ownerDocument || this.tHead.document;
	this.sortTypes = oSortTypes || [];
	var l = cells.length;
  
	var img, c;
	for (var i = 0; i < l; i++) {
		c = cells[i];
		if (this.sortTypes[i] != null && this.sortTypes[i] != "None") {
			img = doc.createElement("img");
			img.src = "/plugin/cavisson/images/blank.gif";
			c.appendChild(img);
			if (this.sortTypes[i] != null)
				c._sortType = this.sortTypes[i];
			if (typeof c.addEventListener != "undefined")
				c.addEventListener("click", this._headerOnclick, false);
			else if (typeof c.attachEvent != "undefined")
				c.attachEvent("onclick", this._headerOnclick);
			else
				c.onclick = this._headerOnclick;
		}
		else
		{
			c.setAttribute( "_sortType", oSortTypes[i] );
			c._sortType = "None";
		}
	}
	this.updateHeaderArrows();
};

// remove arrows and events
SortableTable.prototype.uninitHeader = function () {
	if (!this.tHead) return;
	var cells = this.tHead.rows[0].cells;
	var l = cells.length;
	var c;
	for (var i = 0; i < l; i++) {
		c = cells[i];
		if (c._sortType != null && c._sortType != "None") {
			c.removeChild(c.lastChild);
			if (typeof c.removeEventListener != "undefined")
				c.removeEventListener("click", this._headerOnclick, false);
			else if (typeof c.detachEvent != "undefined")
				c.detachEvent("onclick", this._headerOnclick);
			c._sortType = null;
			c.removeAttribute( "_sortType" );
		}
	}
};

SortableTable.prototype.updateHeaderArrows = function () {
	if (!this.tHead) return;
	var cells = this.tHead.rows[0].cells;
	var l = cells.length;
	var img;
	for (var i = 0; i < l; i++) {
		if (cells[i]._sortType != null && cells[i]._sortType != "None") {
			img = cells[i].lastChild;
			if (i == this.sortColumn)
				img.className = "sort-arrow " + (this.descending ? "descending" : "ascending");
			else
				img.className = "sort-arrow";
		}
	}
};

SortableTable.prototype.headerOnclick = function (e) {
	// find TD element
	var el = e.target || e.srcElement;
	if((e.type != "col-resize"))
	{
	while ((el.tagName != "TH") && (el.tagName != "TD"))
		el = el.parentNode;

	this.sort(SortableTable.msie ? SortableTable.getCellIndex(el) : el.cellIndex);
	}
};

// IE returns wrong cellIndex when columns are hidden
SortableTable.getCellIndex = function (oTd) {
	var cells = oTd.parentNode.childNodes
	var l = cells.length;
	var i;
	for (i = 0; cells[i] != oTd && i < l; i++)
		;
	return i;
};

SortableTable.prototype.getSortType = function (nColumn) {
  return this.sortTypes[nColumn] || "String";
};

// only nColumn is required
// if bDescending is left out the old value is taken into account
// if sSortType is left out the sort type is found from the sortTypes array

SortableTable.prototype.sort = function (nColumn, bDescending, sSortType) {
	if (!this.tBody) return;
	if (sSortType == null)
		sSortType = this.getSortType(nColumn);

	// exit if None
	if (sSortType == "None")
		return;

	if (bDescending == null) {
		if (this.sortColumn != nColumn)
			this.descending = this.defaultDescending;
		else
			this.descending = !this.descending;
	}
	else
		this.descending = bDescending;

	this.sortColumn = nColumn;

	if (typeof this.onbeforesort == "function")
		this.onbeforesort();

	var f = this.getSortFunction(sSortType, nColumn);
	var a = this.getCache(sSortType, nColumn);
	var tBody = this.tBody;

	a.sort(f);

	if (this.descending)
		a.reverse();

	if (SortableTable.removeBeforeSort) {
		// remove from doc
		var nextSibling = tBody.nextSibling;
		var p = tBody.parentNode;
		p.removeChild(tBody);
	}

	// insert in the new order
	var l = a.length;
	for (var i = 0; i < l; i++)
		tBody.appendChild(a[i].element);

	if (SortableTable.removeBeforeSort) {
		// insert into doc
		p.insertBefore(tBody, nextSibling);
	}

	this.updateHeaderArrows();

	this.destroyCache(a);

	if (typeof this.onsort == "function")
		this.onsort();
};

SortableTable.prototype.asyncSort = function (nColumn, bDescending, sSortType) {
	var oThis = this;
	this._asyncsort = function () {
		oThis.sort(nColumn, bDescending, sSortType);
	};
	window.setTimeout(this._asyncsort, 1);
};

SortableTable.prototype.getCache = function (sType, nColumn) {
	if (!this.tBody) return [];
	var rows = this.tBody.rows;
	var l = rows.length;
	var a = new Array(l);
	var r;
	for (var i = 0; i < l; i++) {
		r = rows[i];
		a[i] = {
			value:		this.getRowValue(r, sType, nColumn),
			element:	r
		};
	};
	return a;
};

SortableTable.prototype.destroyCache = function (oArray) {
	var l = oArray.length;
	for (var i = 0; i < l; i++) {
		oArray[i].value = null;
		oArray[i].element = null;
		oArray[i] = null;
	}
};

SortableTable.prototype.getRowValue = function (oRow, sType, nColumn) {
	// if we have defined a custom getRowValue use that
	if (this._sortTypeInfo[sType] && this._sortTypeInfo[sType].getRowValue)
		return this._sortTypeInfo[sType].getRowValue(oRow, nColumn);

	var s;
	var c = oRow.cells[nColumn];
	if (typeof c.innerText != "undefined")
		s = c.innerText;
	else
		s = SortableTable.getInnerText(c);
	return this.getValueFromString(s, sType);
};

SortableTable.getInnerText = function (oNode) {
	var s = "";
	var cs = oNode.childNodes;
	var l = cs.length;
	for (var i = 0; i < l; i++) {
		switch (cs[i].nodeType) {
			case 1: //ELEMENT_NODE
				s += SortableTable.getInnerText(cs[i]);
				break;
			case 3:	//TEXT_NODE
				s += cs[i].nodeValue;
				break;
		}
	}
	return s;
};

SortableTable.prototype.getValueFromString = function (sText, sType) {
	if (this._sortTypeInfo[sType])
		return this._sortTypeInfo[sType].getValueFromString( sText );
	return sText;
	
	};

SortableTable.prototype.getSortFunction = function (sType, nColumn) {
	if (this._sortTypeInfo[sType])
		return this._sortTypeInfo[sType].compare;
	return SortableTable.basicCompare;
};

SortableTable.prototype.destroy = function () {
	this.uninitHeader();
	var win = this.document.parentWindow;
	if (win && typeof win.detachEvent != "undefined") {	// only IE needs this
		win.detachEvent("onunload", this._onunload);
	}
	this._onunload = null;
	this.element = null;
	this.tHead = null;
	this.tBody = null;
	this.document = null;
	this._headerOnclick = null;
	this.sortTypes = null;
	this._asyncsort = null;
	this.onsort = null;
};

// Adds a sort type to all instance of SortableTable
// sType : String - the identifier of the sort type
// fGetValueFromString : function ( s : string ) : T - A function that takes a
//    string and casts it to a desired format. If left out the string is just
//    returned
// fCompareFunction : function ( n1 : T, n2 : T ) : Number - A normal JS sort
//    compare function. Takes two values and compares them. If left out less than,
//    <, compare is used
// fGetRowValue : function( oRow : HTMLTRElement, nColumn : int ) : T - A function
//    that takes the row and the column index and returns the value used to compare.
//    If left out then the innerText is first taken for the cell and then the
//    fGetValueFromString is used to convert that string the desired value and type

SortableTable.prototype.addSortType = function (sType, fGetValueFromString, fCompareFunction, fGetRowValue) {
	this._sortTypeInfo[sType] = {
		type:				sType,
		getValueFromString:	fGetValueFromString || SortableTable.idFunction,
		compare:			fCompareFunction || SortableTable.basicCompare,
		getRowValue:		fGetRowValue
	};
};

// this removes the sort type from all instances of SortableTable
SortableTable.prototype.removeSortType = function (sType) {
	delete this._sortTypeInfo[sType];
};

SortableTable.basicCompare = function compare(n1, n2) {
	if (n1.value < n2.value)
		return -1;
	if (n2.value < n1.value)
		return 1;
	return 0;
};

SortableTable.idFunction = function (x) {
	return x;
};

SortableTable.toUpperCase = function (s) {
	return s.toUpperCase();
};


/*****SortableTable.toDate = function (s) {
	
  var date = s.split(" ");
  var parts = date[0].split("/");;
	var d = new Date(0);
	d.setFullYear(parts[2]);
	d.setDate(parts[1]);
	d.setMonth(parts[0]);
	//d.setDate(parts[0]);
	//d.setMonth(parts[1] - 1);
//  alert(d);

	return d.valueOf();

};***********/

////////////////////////////////////////////////////////////
// Format can be in 1) mm/dd/yyyy hh:mm:ss(Ex: 07/15/2009 16:09:59)
//                  2) mm/dd/yy hh:mm:ss(Ex: 07/15/09 16:09:59)
//                  3) mm/dd/yy hh:mm (Ex: 07/15/09 16:09)
//                  4) mm/dd/yyyy (Ex: 07/15/2009)
//                  5) mm/dd/yy (Ex: 07/15/09)
//                  6) mm/dd (Ex: 07/15)
//                  7) NA


SortableTable.toDate = function (s) {
 
  // If date and time is NA 0r 0
  if(s == "NA")
  {
    var Ndate = "000";
    return Ndate;
  }
    
    
  s=trimString(s);  
  // if date and time both are present. Split the time and date.
  var date = s.split(" ");
  var parts = "";
  
  //Split date from forward slash
  if(date[0].indexOf("-") > -1)
  {
    parts = date[0].split("-");
  }
  else
  {
    parts = date[0].split("/");
  }
  var d = new Date(0);
  
  //setYear() Sets the year in the Date object (two or four digits). Use setFullYear() instead !!
  //d.setFullYear(parts[2]);
  
  //format in mm/dd set year 2009
  if(parts.length == 2)
  {
    //d.setYear(2009);
    var myDate=new Date(); 
    var curYY = myDate.getFullYear();
    d.setYear(curYY);
  }
  else  
    d.setYear(parts[2]);

  d.setDate(parts[1]);
  d.setMonth(parts[0]);

  //return d.valueOf();

  //If time is present it will go in this condition
  if(date.length == 2)
  { 
    var t = new Date(0);
    
    // Calling toTime method which are common method
    var time = SortableTable.toTime(date[1]);
    
    t.setSeconds(time);
    
    //valueOf: convert date and time in milliseconds (Returns the primitive value of a Date object)
    var tmpDate = d.valueOf();
    var tmpTime = t.valueOf();

    var tmpDateTime = tmpDate + tmpTime;
    return tmpDateTime; //return sum of date and time
  }
  else
    return d.valueOf();  //return only date
};

//////////////////////////////////////////////////////////////

//06/1/06:Abhishek - add this for number sorting when '-' occur in table
SortableTable.toNumber = function (s) {
  s= s.replace(/\,/g,'');
  var num = s.split(" ");	
  if(num[0] == '-')
  {
    num[0] = '0';
  }
  else if(num[0] == 'Auto')
  {
    num[0] = '0';
  }
  
  var numBin = toBin(num[0]);
  var resultNumber = parseInt(numBin, 2);

  return resultNumber;
};

SortableTable.toIP = function (s) {
	
  var num = s.split(" ");
  if(num[0] == '-')
    num[0] = '0.0.0.0';
  
  var num1 = num[0].split(".");
  var numBin = toBin(num1[0]) + toBin(num1[1]) + toBin(num1[2]) + toBin(num1[3]);
  var resultIp = parseInt(numBin, 2);
  return resultIp;
};

// 10/07/06:Abhishek - Add this to sort time HH:MM:SS
SortableTable.toTime = function (s) {
  if((s == "NA") || (s == "0"))
  {
    num = -1;
    return num;
  }

  //Time in hh:mm. Here we set sec = 0
  var num = s.split(":");
 
  if(num.length == 2)
  {
    num[2] = "0";
  }
  
  var numBin = (Math.abs(eval(num[0]* 60 * 60)) + Math.abs(eval(num[1] * 60)) + Math.abs(eval(num[2]))); 
  return numBin;
};


// 10/07/06:Abhishek - Add this to sort time HH:MM:SS
SortableTable.toNumColon = function (s) {
  var num = s.split(":"); 
  if(num[0].length < 8)
  {
    for(var i = num[0].length; i < 8; i++)
    {
      num[0] = "0" + num[0];
    }
  }

  num[0] = num[0] + "1";
  
  if(num[1].length < 8)
  {
    for(var i = num[1].length; i < 8; i++)
    {
      num[1] = "0" + num[1];
    }
  }

  num[1] = num[1] + "1";
  return SortableTable.toUpperCase(num[0] + "" + num[1]);
};


//22x44
SortableTable.toNumSizeCross = function (s) {

  var num = s.split("x"); 
  if(num.length == 1)
    return;

  if(num[0].length < 8)
  {
    for(var i = num[0].length; i < 8; i++)
    { 
      num[0] = "0" + trimString(num[0]);
    }
  }

  num[0] = num[0] + "1";
  
  if(num[1].length < 8)
  {
    for(var i = num[1].length; i < 8; i++)
    {
      num[1] = "0" + trimString(num[1]);
    }
  }

  num[1] = num[1] + "1";

  return SortableTable.toUpperCase(num[0] + "" + num[1]);
};

// to sort decimal numbers.
SortableTable.toDecimalNew = function(s) {
s = s.replace(/,/g, "");
  var result;
  if(s != "")
   result = parseFloat(s);
  else
   result = s;		
  return result;
};
SortableTable.toDecimalNum = function (s) {
  s = s.replace("%", "");
  s = s.replace(",", "");
  s=trimString(s)

  if(s == "-")
    return 0;
  if(s == "Different")
    return 0;
  if(s == "Same")
    return 0;

  s = s.replace("-", "0");

  var num = s.split(".");
  if(num[0].length < 8)
  {
    for(var i = num[0].length; i < 8; i++)
    {
      num[0] = "0" + num[0];
    }
  }

  num[0] = num[0] + "1";
  if(num.length == 1)
    num[1] = 0;
  if(num[1].length < 8)
  {
    for(var i = num[1].length; i < 8; i++)
    {
      num[1] = "0" + num[1];
    }
  }

  num[1] = num[1] + "1";
  return SortableTable.toUpperCase(num[0] + "" + num[1]);
};


//To sort file according to file size. Added by Pratik Mishra
SortableTable.toFileSize= function (s) {
  
  var data = s.split(" ");
  var num = "";
  
  if(trimString(data[1]) == "KB")
    num = Math.abs(eval(data[0]*1024));
  else if(trimString(data[1])  == "MB")
    num = Math.abs(eval(data[0]*1024*1024));  
  else if(trimString(data[1])  == "GB")
  {
    num = Math.abs(eval(data[0]*1024*1024*1024));
  }
  else 
   num = Math.abs(eval(data[0]));
  
  
  var numBin = toBin(num);
  var resultNumber = parseInt(numBin, 2);
  
  return resultNumber;
};


// Decimal to binary, returns an eight character string
function toBin(inVal)
{
  base = 2 ;
  num = parseInt(inVal);
  binNum = num.toString(base);
  // pad leading spaces with "0"
  binNum = padTextPrefix(binNum, "0", 8) ;

  return binNum
}

//To Sort Floating point digits
SortableTable.toDecimal = function(s) {

  var result;
  if(s != "")
   result = parseFloat(s);
  else
   result = s;		
  return result;
};

function padTextPrefix (InString, PadChar, DefLength)
{
  if (InString.length >= DefLength)
     return (InString);
  OutString = InString
  
  for (Count = InString.length; Count < DefLength; Count++)
  {
     OutString = PadChar + OutString;
  }
  return (OutString);
}

function restoreCheckBoxesForSorting(st)
{
  //This code for restoring checkbox value while sorting
  // IE does not remember input values when moving DOM elements
  if (/MSIE/.test(navigator.userAgent)) 
  {
    // backup check box values
    st.onbeforesort = function () 
    {
      var table = st.element;
      var inputs = table.getElementsByTagName("INPUT");
      var l = inputs.length;
      for (var i = 0; i < l; i++) 
      {
        inputs[i].parentNode.parentNode._checked = inputs[i].checked;
      }
    }
    // restore check box values
    st.onsort = function () 
    {
      var table = st.element;
      var inputs = table.getElementsByTagName("INPUT");
      var l = inputs.length;
      for (var i = 0; i < l; i++) 
      {
        inputs[i].checked = inputs[i].parentNode.parentNode._checked;
      }
    }
  }
}

