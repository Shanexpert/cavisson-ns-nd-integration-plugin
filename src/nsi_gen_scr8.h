
//Args: obj-type obj-type obj-type obj-name user-id obj-type obj-start-time
char * partA = " <HEAD>\n\
                <meta content=\"Microsoft Visual Studio 7.0\" name=\"GENERATOR\">\n\
                <meta content=\"C#\" name=\"CODE_LANGUAGE\">\n\
                <meta content=\"JavaScript\" name=\"vs_defaultClientScript\">\n\
                <meta content=\"http://schemas.microsoft.com/intellisense/ie5\" name=\"vs_targetSchema\">\n\
		 <script language=\"javascript\">\n\
                        view = \"WF\" \n\
                </script>\n\
		<script language=\"JavaScript\" src=\"chartFX/popup.js\" type=\"text/javascript\"></script>\n\
                <LINK href=\"css/tempstyle.css\" type=\"text/css\" rel=\"stylesheet\">\n\
                <LINK href=\"/netstorm/css/Netstorm.css\" type=\"text/css\" rel=\"stylesheet\">\n\
        </HEAD>\n\
<body leftMargin=\"0\" MS_POSITIONING=\"GridLayout\">\n\
\n\
			<div id=\"theChartandHeader\">\n\
				<table class=\"black-2\" align=\"center\" cellSpacing=\"0\" cellPadding=\"0\" width=\"98%%\" bgColor=\"#999999\" border=\"0\">\n\
					<tr style=\"PADDING-LEFT: 10px; PADDING-TOP: 10px\" bgColor=\"#ffffff\">\n\
						<td align=\"left\"><span id=\"tranheader\" class=\"black-2\" align=\"left\"><b>Selected %s:</b>&nbsp;&nbsp;</span><span id=\"tranLabel\" class=\"black-2\" align=\"left\">%s</span><br>\n\
							<span id=\"nodeheader\" class=\"black-2\"><b>User Id:</b>&nbsp;&nbsp;</span><span id=\"nodeLabel\" class=\"black-2\">%s:%s</span><br>\n\
							<span id=\"testtimeheader\" class=\"black-2\"><b>%s Start Time (HH:MM:SS.ms):</b>&nbsp;&nbsp;</span><span id=\"testtime\" class=\"black-2\">%s (from test start time)</span><br>\n\
							<br>\n\
							</td>\n\
					</tr>\n\
					<tr>\n\
						<td><span id=\"chart\"><table border=0 cellpadding=2 cellspacing=1 bgcolor='#cccccc' class='black-1' width='100%%>\n\
					<tr bgcolor='#dfdfdf'> <td colspan=2>&nbsp;</td></tr>\n\n";

//for start-time = 0
//print args: page-num, page-name, page-intvl-pix, page-num, page-name, page-intvl-sec
char *pagebarA ="\
	<tr><td bgcolor='#dfdfdf' align='right'>Page %d: %s</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/colors/0.gif' border=0 height=10 width=%d  onmouseout=\"popupoff()\" onmouseover=\"popupon('Page Download Time<br>Page %d: %s<br>%6.3f',event)\"></td>\n\
	</tr>\n";

//for start-time != 0
//print args: page-num, page-name, page-start-pix page-intvl-pix, page-num, page-name, page-intvl-sec
char *pagebarB = "\
	<tr><td bgcolor='#dfdfdf' align='right'>Page %d: %s</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/utilities/dotclear.gif' border=0 height=10 width=%d><img src='images/colors/0.gif' border=0 height=10 width=%d onmouseout=\"popupoff()\" onmouseover=\"popupon('Page Download Time<br>Page %d: %s<br>%6.3f',event)\"></td>\n\
	</tr>\n";

//args: url-name
char *urlbarBegin = "\
	<tr><td bgcolor='#dfdfdf' align='right'>Conn %d:%s</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\">";

//args: start-pix
char *urlbarStart = "<img src='images/utilities/dotclear.gif' border=0 height=10 width=%d>";
//args: pix, url, sec
char *urlbarConnect = "<img src='images/colors/1.gif' border=0 height=10 width=%d onmouseout=\"popupoff()\" onmouseover=\"popupon('Connect Time<br>%s<br>%6.3f',event)\">";

//args: pix, url, sec
char *urlbarSSL = "<img src='images/colors/2.gif' border=0 height=10 width=%d onmouseout=\"popupoff()\" onmouseover=\"popupon('SSL Time<br>%s<br>%6.3f',event)\">";

//args: pix, url, sec
char *urlbarWrite = "<img src='images/colors/3.gif' border=0 height=10 width=%d onmouseout=\"popupoff()\" onmouseover=\"popupon('Request Send Time<br>%s<br>%6.3f',event)\">";

//args: pix, url, sec
char *urlbarFirst = "<img src='images/colors/4.gif' border=0 height=10 width=%d onmouseout=\"popupoff()\" onmouseover=\"popupon('First Bytes Time<br>%s<br>%6.3f',event)\">";

//args: pix, url, sec
char *urlbarContent = "<img src='images/colors/5.gif' border=0 height=10 width=%d onmouseout=\"popupoff()\" onmouseover=\"popupon('content Download Time<br>%s<br>%6.3f',event)\">";

char *urlbarEnd = "</td>\n\
	</tr>\n";

/* 
	<tr><td bgcolor='#dfdfdf' align='right'>Page 0: https://mail.e2open.com/exchange</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/colors/0.gif' border=0 height=10 width=329 onmouseout=\"popupoff()\" onmouseover=\"popupon('Page Download Time<br>Page 0: https://mail.e2open.com/exchange<br>0.274',event)\"></td>\n\
	</tr>\n\
	<tr><td bgcolor='#dfdfdf' align='right'>Connection 0 - mail.e2open.com (129.41.18.190)</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/colors/1.gif' border=0 height=10 width=1 onmouseout=\"popupoff()\" onmouseover=\"popupon('DNS Time<br>Connection 0 - mail.e2open.com (129.41.18.190)<br>0.001',event)\"><img src='images/colors/2.gif' border=0 height=10 width=29 onmouseout=\"popupoff()\" onmouseover=\"popupon('Connect Time<br>Connection 0 - mail.e2open.com (129.41.18.190)<br>0.024',event)\"></td>\n\
	</tr>\n\
	<tr><td bgcolor='#dfdfdf' align='right'>https://mail.e2open.com/exchange</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/utilities/dotclear.gif' border=0 height=10 width=66><img src='images/colors/4.gif' border=0 height=10 width=31 onmouseout=\"popupoff()\" onmouseover=\"popupon('First Byte Time<br>https://mail.e2open.com/exchange<br>0.026',event)\"><img src='images/colors/5.gif' border=0 height=10 width=232 onmouseout=\"popupoff()\" onmouseover=\"popupon('Content Time<br>https://mail.e2open.com/exchange<br>0.193',event)\"></td>\n\
	</tr>\n\
	<tr><td bgcolor='#dfdfdf' align='right'>Page 1: https://mail.e2open.com/exchange/logon.asp</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/utilities/dotclear.gif' border=0 height=10 width=329><img src='images/colors/0.gif' border=0 height=10 width=271 onmouseout=\"popupoff()\" onmouseover=\"popupon('Page Download Time<br>Page 1: https://mail.e2open.com/exchange/logon.asp<br>0.226',event)\"></td>\n\
	</tr>\n\
	<tr><td bgcolor='#dfdfdf' align='right'>https://mail.e2open.com/exchange/logon.asp</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/utilities/dotclear.gif' border=0 height=10 width=329><img src='images/colors/4.gif' border=0 height=10 width=50 onmouseout=\"popupoff()\" onmouseover=\"popupon('First Byte Time<br>https://mail.e2open.com/exchange/logon.asp<br>0.042',event)\"><img src='images/colors/5.gif' border=0 height=10 width=7 onmouseout=\"popupoff()\" onmouseover=\"popupon('Content Time<br>https://mail.e2open.com/exchange/logon.asp<br>0.006',event)\"></td>\n\
	</tr>\n\
	<tr><td bgcolor='#dfdfdf' align='right'>https://mail.e2open.com/exchange/back.jpg</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/utilities/dotclear.gif' border=0 height=10 width=386><img src='images/colors/4.gif' border=0 height=10 width=30 onmouseout=\"popupoff()\" onmouseover=\"popupon('First Byte Time<br>https://mail.e2open.com/exchange/back.jpg<br>0.025',event)\"><img src='images/colors/5.gif' border=0 height=10 width=6 onmouseout=\"popupoff()\" onmouseover=\"popupon('Content Time<br>https://mail.e2open.com/exchange/back.jpg<br>0.005',event)\"></td>\n\
	</tr>\n\
	<tr><td bgcolor='#dfdfdf' align='right'>Connection 1 - mail.e2open.com (129.41.18.190)</td><td bgcolor='#ffffff' style=\"width:600px\"><img src='images/utilities/dotclear.gif' border=0 height=10 width=388><img src='images/colors/2.gif' border=0 height=10 width=37 onmouseout=\"popupoff()\" onmouseover=\"popupon('Connect Time<br>Connection 1 - mail.e2open.com (129.41.18.190)<br>0.031',event)\"></td>\n\
	</tr>\n\
	<tr><td bgcolor='#dfdfdf' align='right'>https://mail.e2open.com/exchange/part2.gif</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/utilities/dotclear.gif' border=0 height=10 width=424><img src='images/colors/4.gif' border=0 height=10 width=30 onmouseout=\"popupoff()\" onmouseover=\"popupon('First Byte Time<br>https://mail.e2open.com/exchange/part2.gif<br>0.025',event)\"><img src='images/colors/5.gif' border=0 height=10 width=8 onmouseout=\"popupoff()\" onmouseover=\"popupon('Content Time<br>https://mail.e2open.com/exchange/part2.gif<br>0.007',event)\"></td>\n\
	</tr>\n\
	<tr><td bgcolor='#dfdfdf' align='right'>https://mail.e2open.com/exchange/part1.gif</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/utilities/dotclear.gif' border=0 height=10 width=462><img src='images/colors/4.gif' border=0 height=10 width=31 onmouseout=\"popupoff()\" onmouseover=\"popupon('First Byte Time<br>https://mail.e2open.com/exchange/part1.gif<br>0.026',event)\"><img src='images/colors/5.gif' border=0 height=10 width=107 onmouseout=\"popupoff()\" onmouseover=\"popupon('Content Time<br>https://mail.e2open.com/exchange/part1.gif<br>0.089',event)\"></td>\n\
	</tr>\n\
	<tr><td bgcolor='#dfdfdf' align='right'>https://mail.e2open.com/exchange/msie.gif</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/utilities/dotclear.gif' border=0 height=10 width=463><img src='images/colors/4.gif' border=0 height=10 width=30 onmouseout=\"popupoff()\" onmouseover=\"popupon('First Byte Time<br>https://mail.e2open.com/exchange/msie.gif<br>0.025',event)\"><img src='images/colors/5.gif' border=0 height=10 width=38 onmouseout=\"popupoff()\" onmouseover=\"popupon('Content Time<br>https://mail.e2open.com/exchange/msie.gif<br>0.032',event)\"></td>\n\
	</tr>\n\
	<tr><td bgcolor='#dfdfdf' align='right'>https://mail.e2open.com/exchange/msprod.gif</td>\n\
	    <td bgcolor='#ffffff' style=\"width:600px\"><img src='images/utilities/dotclear.gif' border=0 height=10 width=533><img src='images/colors/4.gif' border=0 height=10 width=32 onmouseout=\"popupoff()\" onmouseover=\"popupon('First Byte Time<br>https://mail.e2open.com/exchange/msprod.gif<br>0.027',event)\"><img src='images/colors/5.gif' border=0 height=10 width=24 onmouseout=\"popupoff()\" onmouseover=\"popupon('Content Time<br>https://mail.e2open.com/exchange/msprod.gif<br>0.020',event)\"></td>\n\
	</tr>\n
*/

char *partB ="\
	<tr bgcolor='#dfdfdf'><td>&nbsp;</td><td><table border=0 cellpadding=0 cellspacing=0 class='black-0'>\n\
	<tr>\n\
		<td><img src='images/utilities/dotclear.gif' border=0 height=2 width=59><img src='images/utilities/pixel_black.gif' border=0 height=2 width=1></td>\n\
	   	<td><img src='images/utilities/dotclear.gif' border=0 height=2 width=59><img src='images/utilities/pixel_black.gif' border=0 height=2 width=1></td>\n\
	  	<td><img src='images/utilities/dotclear.gif' border=0 height=2 width=59><img src='images/utilities/pixel_black.gif' border=0 height=2 width=1></td>\n\
	   	<td><img src='images/utilities/dotclear.gif' border=0 height=2 width=59><img src='images/utilities/pixel_black.gif' border=0 height=2 width=1></td>\n\
	      	<td><img src='images/utilities/dotclear.gif' border=0 height=2 width=59><img src='images/utilities/pixel_black.gif' border=0 height=2 width=1></td>\n\
		<td><img src='images/utilities/dotclear.gif' border=0 height=2 width=59><img src='images/utilities/pixel_black.gif' border=0 height=2 width=1></td>\n\
		<td><img src='images/utilities/dotclear.gif' border=0 height=2 width=59><img src='images/utilities/pixel_black.gif' border=0 height=2 width=1></td>\n\
		<td><img src='images/utilities/dotclear.gif' border=0 height=2 width=59><img src='images/utilities/pixel_black.gif' border=0 height=2 width=1></td>\n\
		<td><img src='images/utilities/dotclear.gif' border=0 height=2 width=59><img src='images/utilities/pixel_black.gif' border=0 height=2 width=1></td>\n\
		<td><img src='images/utilities/dotclear.gif' border=0 height=2 width=59><img src='images/utilities/pixel_black.gif' border=0 height=2 width=1></td>\n\
	</tr>";

#if 0
	<tr><td align='right'>0.05</td>\n\
	    <td align='right'>0.1</td>\n\
	    <td align='right'>0.15</td>\n\
	    <td align='right'>0.2</td>\n\
	    <td align='right'>0.25</td>\n\
	    <td align='right'>0.3</td>\n\
	    <td align='right'>0.35</td>\n\
	    <td align='right'>0.4</td>\n\
	    <td align='right'>0.45</td>\n\
	    <td align='right'>0.5</td>\n\
       </tr>
#endif

char *partC1 = "\
		<td align='center' colspan=10>seconds</td>\n\
	</tr>\n\
	</table></td></tr>\n\
		<tr bgcolor='#ffffff'><td colspan=2 align='center'><table border=0 cellpadding=0 cellspacing=5 class='black-1'><tr><td><img src='images/colors/0.gif' border=0 height=10 width=10>&nbsp;Page Download Time</td><td><img src='images/colors/1.gif' border=0 height=10 width=10>&nbsp;Connect Time</td><td><img src='images/colors/2.gif' border=0 height=10 width=10>&nbsp;SSL Time</td></tr>\n\
		<tr><td><img src='images/colors/3.gif' border=0 height=10 width=10>&nbsp;Request Sent Time</td><td><img src='images/colors/4.gif' border=0 height=10 width=10>&nbsp;First Byte Time</td><td><img src='images/colors/5.gif' border=0 height=10 width=10>&nbsp;Content Time</td></tr></table></td></tr></table></span></td>\n\
		</tr>\n\
	</table>\n\
	</div>\n\
	<div style=\"PADDING-TOP: 20px\"><table id=\"objTable\" class=\"black-2\" align=\"center\" cellspacing=\"1\" cellpadding=\"0\" border=\"0\" bgcolor=\"#999999\" width=\"98%%\">\n\
	<tr id=\"objHeader\" bgcolor=\"#EFEFEF\">\n\
		<td class=\"tdpad10left\"><b>Object Response Time Data</b></td>\n\
	</tr><tr bgcolor=\"White\">\n\
	<td>\n\
	<table border=\"0\" cellpadding=\"10\" cellspacing=\"0\" class=\"black-2\">\n\
	<tr>\n\
	<td>\n";

//Args: obj-type time-sec bytes
char * row_header = "<span id=\"transRT\"><b>%s Response Time:</b>&nbsp;%6.3f<br></span><span id=\"transBytes\"><b>Total Bytes:</b>&nbsp;%d</span>\n";

/*
	<span id=\"transRT\"><b>Transaction Response Time:</b>&nbsp;0.5<br></span><span id=\"transBytes\"><b>Total Bytes:</b>&nbsp;52244</span>\n\
*/
char *partC2 = "\
	</td>\n\
	</tr>\n\
	</table>\n\
	<table class=\"black-1\" cellspacing=\"1\" cellpadding=\"2\" rules=\"all\" bordercolor=\"White\" border=\"0\" id=\"dg\" bgcolor=\"White\" width=\"100%%\">\n\
	<tr valign=\"Bottom\">\n\
	<td align=\"left\"><b>Object Name</b></td><td align=\"left\"><b>Status</b></td><td align=\"left\"><b>HTTP Code</b></td><td align=\"left\"><b>Connection Reused?</b></td><td align=\"left\"><b>SSL Reused?</b></td><td align=\"right\"><b>Total Time</b></td><td align=\"right\"><b>Connect Time</b></td><td align=\"right\"><b>SSL time</b></td><td align=\"right\"><b>Request Time</b></td><td align=\"right\"><b>1st Byte</b></td><td align=\"right\"><b>Content Download</b></td><td align=\"right\"><b>Total Bytes</b></td>\n\
	</tr>";

char *evenhead = "\t<tr bgcolor=\"#CCCCCC\">\n";
char *oddhead = "<tr bgcolor=\"#EFEFEF\">\n";

//args: page_num, page_name, time-sec, bytes
char *pagerow = "\t\t<td>Page %d: %s</td><td>0</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td align=\"right\">%6.3f</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td align=\"right\">%d</td>\n\t</tr>\n";

/*
	<tr bgcolor=\"#CCCCCC\">\n\
		<td>Page 0: https://mail.e2open.com/exchange</td><td>text/html</td><td>200</td><td>0.219</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td>\n\
	</tr>\n\
	<tr bgcolor=\"#EFEFEF\">\n\
		<td>Connection 0 - mail.e2open.com (129.41.18.190)</td><td>Connection</td><td>&nbsp;</td><td>0.490</td><td>0.001</td><td>0.024</td><td>0.030</td><td>&nbsp;</td><td>&nbsp;</td><td>37583</td>\n\
	</tr>\n\
	<tr bgcolor=\"#CCCCCC\">\n\
		<td>https://mail.e2open.com/exchange</td><td>text/html</td><td>200</td><td>0.219</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>0.026</td><td>0.193</td><td>218</td>\n\
	</tr>\n\
	<tr bgcolor=\"#EFEFEF\">\n\
		<td>Page 1: https://mail.e2open.com/exchange/logon.asp</td><td>text/html</td><td>200</td><td>0.048</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td>\n\
	</tr>\n\
	<tr bgcolor=\"#CCCCCC\">\n\
		<td>https://mail.e2open.com/exchange/logon.asp</td><td>text/html</td><td>200</td><td>0.048</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>0.042</td><td>0.006</td><td>4574</td>\n\
	</tr>\n\
	<tr bgcolor=\"#EFEFEF\">\n\
		<td>https://mail.e2open.com/exchange/back.jpg</td><td>image/jpeg</td><td>200</td><td>0.030</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>0.025</td><td>0.005</td><td>3537</td>\n\
	</tr>\n\
	<tr bgcolor=\"#CCCCCC\">\n\
		<td>Connection 1 - mail.e2open.com (129.41.18.190)</td><td>Connection</td><td>&nbsp;</td><td>0.177</td><td>&nbsp;</td><td>0.031</td><td>0.031</td><td>&nbsp;</td><td>&nbsp;</td><td>14661</td>\n\
	</tr>\n\
	<tr bgcolor=\"#EFEFEF\">\n\
		<td>https://mail.e2open.com/exchange/part2.gif</td><td>image/gif</td><td>200</td><td>0.032</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>0.025</td><td>0.007</td><td>4266</td>\n\
	</tr>\n\
	<tr bgcolor=\"#CCCCCC\">\n\
		<td>https://mail.e2open.com/exchange/part1.gif</td><td>image/gif</td><td>200</td><td>0.115</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>0.026</td><td>0.089</td><td>14661</td>\n\
	</tr>\n\
	<tr bgcolor=\"#EFEFEF\">\n\
		<td>https://mail.e2open.com/exchange/msie.gif</td><td>image/gif</td><td>200</td><td>0.057</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>0.025</td><td>0.032</td><td>9132</td>\n\
	</tr>\n\
	<tr bgcolor=\"#CCCCCC\">\n\
		<td>https://mail.e2open.com/exchange/msprod.gif</td><td>image/gif</td><td>200</td><td>0.047</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>0.027</td><td>0.020</td><td>15856</td>\n\
	</tr>\n


*/

char *partD = "\
	</table></td>\n\
	</tr>\n\
	</table></div>\n\
	<div id=\"popup\" style=\"Z-INDEX: 101; VISIBILITY: hidden; POSITION: absolute\"></div>\n\
	</body>\n\
</HTML>";
