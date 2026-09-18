// need listener for when python is ready and can transfer data
document.addEventListener('DOMContentLoaded', () => {
    getData();
})

async function getData(retries = 3, delay = 5000){
    try{
        console.log("Abt to fetch...");
        const response = await fetch('http://localhost:5000/data')
        if (!response.ok) {
          throw new Error(`HTTP error! status: ${response.status}`);
        }
        const data = await response.json();
        console.log("Received package!");
        // console.log("Datais: ");
        // console.log(data);

        const driver_ids = data['drivers'];
        const driver_points = data['points'];

        console.log('Calling forEach on:', driver_ids);
        console.log('Type:', typeof driver_ids);
        console.log('Array.isArray:', Array.isArray(driver_ids));
        driver_ids.forEach(item => console.log(item));

        console.log('Calling forEach on:', driver_points);
        console.log('Type:', typeof driver_points);
        console.log('Array.isArray:', Array.isArray(driver_points));
        driver_points.forEach(item => console.log(item));

        let newDriverHTML = "";
        driver_ids.forEach(driver => {
            newDriverHTML += `<li>${driver}</li>`;
        });

        let newPointsHTML = "";
        driver_points.forEach(points => {
            newPointsHTML += `<li>${points}</li>`
        })

        "list-style-type: none; display: inline-block;"
        driverList = document.getElementById("drivers_list");
        driverList.innerHTML = newDriverHTML;
        driverList.classList.add('info_list');

        pointsList = document.getElementById("points_list");
        pointsList.innerHTML = newPointsHTML;
        points_list.classList.add("info_list")
        console.log("Updated lists....");
    }
    catch (error) {
        console.error("Failed to update lists:", error);
        if (retries > 0) {
            console.log(`Retrying... (${retries} attempts left)`);
            setTimeout(() => getData(retries - 1, delay), delay);
        } else {
            console.error("All retries failed. Please try again.");
        }
    }
}