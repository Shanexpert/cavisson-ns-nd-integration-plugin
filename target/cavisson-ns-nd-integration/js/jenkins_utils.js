function showOrHideReport()
{
  var element = document.getElementById("reportTableId"); 
  var imgElement = document.getElementById("showOrHideReportImgId");
  
  if(element.style.display == "none")
  {
    element.style.display = "table";
    imgElement.src = "/plugin/cavisson/images/arrow_d.png";
  }
  else
  {
    element.style.display = "none";
    imgElement.src = "/plugin/cavisson/images/arrow_r.png";
  }
}

function setupTestRunsTableSortWithBaseLine()
{
  var st = new SortableTable(document.getElementById("reportID"), ["String",  "Number", "Number"]);
  // add sort types
  var sTypePro = SortableTable.prototype;
  sTypePro.addSortType("String", SortableTable.toUpperCase);
  sTypePro.addSortType("Number", Number);
  sTypePro.addSortType("Number", Number);

  //This function is called to restore the values of checkboxes in table while sorting,if there r no chkbox then there is no use of this function,parameter pass is st.
  restoreCheckBoxesForSorting(st);
}


function setupTestRunsTableSort()
{
  var cols = document.getElementById("reportID").rows[0].cells;
  var colsType = new Array();
  for(var i = 0 ; i < cols.length ; i++)
  {
    if(cols[i].innerHTML.indexOf("Metric Under Test") > -1)
    {
      colsType[i] = "String";
    }
    else if(cols[i].innerHTML.indexOf("Value") > -1)
    {
      colsType[i] = "Number";
    }
    else if(cols[i].innerHTML.indexOf("") > -1)
    {
      colsType[i] = "String";
    }
    else if(cols[i].innerHTML.indexOf("SLA") > -1)
    {
      colsType[i] = "Number";
    }
    else if(cols[i].innerHTML.indexOf("Initial") > -1)
    {
      colsType[i] = "Number";
    }
    else if(cols[i].innerHTML.indexOf("Baseline") > -1)
    {
      colsType[i] = "Number";
    }
    else if(cols[i].innerHTML.indexOf("Previous") > -1)
    {
      colsType[i] = "Number";
    }
    else if(cols[i].innerHTML.indexOf("Success") > -1)
    {
      colsType[i] = "Number";
    }
  }
  
  var st = new SortableTable(document.getElementById("reportID"), colsType);
  // add sort types
  var sTypePro = SortableTable.prototype;
  for(var j = 0; j < colsType.length; j++)
  {
   if(colsType[j] == "String")
   {
     sTypePro.addSortType("String", SortableTable.toUpperCase);
   }
   else
   {
     sTypePro.addSortType("Number", Number);
   }
  }
  //This function is called to restore the values of checkboxes in table while sorting,if there r no chkbox then there is no use of this function,parameter pass is st.
  restoreCheckBoxesForSorting(st);
}


