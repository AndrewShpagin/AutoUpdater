window.setInterval(() => {
    fetch("/ping");
}, 1000);
async function sendObject(object) {
    try {
        const body = await fetch("/exchange", {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(object)
        });
        const str = await body.text();
        const res = JSON.parse(str);
        return res || {};
    } catch (error) {
        console.log(error);
    }
    return {};
}