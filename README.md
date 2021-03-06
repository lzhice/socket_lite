# socket_lite

<p>Windows <img src="https://ci.appveyor.com/api/projects/status/gupob5nj63onam9l?svg=true"/><p>
<p>Linx/Mac <img src="https://travis-ci.org/smasherprog/socket_lite.svg?branch=master"/><p>

<p>This library is a cross-platform networking library with a similiar API to ASIO.</p>

<h3>Platform Benchmarks vs ASIO as a baseline</h3>
<p>ST = Single Threaded  MT = Multi-threaded</p>
<p>Negative numbers mean ASIO is faster, where positive numbers mean Socket_Lite is faster</p>
<table>
 <thead>
   <tr>
     <th></th>
     <th>Windows Debug</th> 
     <th>Windows Release</th> 
     <th>Linux Debug</th>
     <th>Linux Release</th>
   </tr>
  </thead>
  <tbody>
     <tr>
     <td>ST Connections Per Second</td>
      <td>2X</td>
     <td>2X</td>
     <td>2X</td>
     <td>2X</td>
    </tr>
  <tr>
     <td>ST Echos Per Second</td>
     <td>2.5X</td> 
    <td>1.75X</td> 
   <td>1.5X</td>   
   <td>1.75X</td> 
    </tr>
   <tr>
     <td>MT Echos Per Second</td>
     <td>4X</td>
     <td>2.5X</td>
     <td>2X</td>    
      <td>2X</td>    
    </tr>
      <tr>
     <td>ST Bulk Transfer Bytes Per Second</td>
     <td>0% CPU usage is half</td>
     <td>0% CPU usage is half</td>
     <td>1.5X</td>    
      <td>1.5X</td>    
    </tr>
</tbody>
</table>
